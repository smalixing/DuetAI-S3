/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

#include "wifi_sta.h"

static const char *TAG = "wifi_sta";

// WiFi access point credentials
#define WIFI_SSID               "HUAWEI-1F3M1W"
#define WIFI_PSK                "test123456"

// How long wifi_sta_start() waits for an IP before giving up
#define WIFI_GOT_IP_TIMEOUT_MS  (15 * 1000)

// NTP server and how long wifi_sta_sync_time() waits for the clock to be set
#define SNTP_SERVER             "pool.ntp.org"
#define SNTP_SYNC_TIMEOUT_MS    (15 * 1000)
// A synced clock must be at least this (year 2022) to be considered valid
#define SNTP_MIN_VALID_EPOCH    (1640995200)

// Event group bits set from the WiFi/IP event handler
#define WIFI_CONNECTED_BIT      (0x01 << 0)
#define WIFI_FAIL_BIT           (0x01 << 1)

static EventGroupHandle_t s_wifi_event_group = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Keep retrying; the AP may be transiently unavailable.
        ESP_LOGW(TAG, "Connect 'wifi_sta': disconnected, retrying");
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Start 'wifi_sta': got ip " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_sta_start(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start 'wifi_sta' failed: nvs init %s", esp_err_to_name(ret));
        return ret;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Start 'wifi_sta' failed: event group alloc");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PSK,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Start 'wifi_sta': connecting to '%s'", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_GOT_IP_TIMEOUT_MS));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Start 'wifi_sta' failed: no ip within %d ms", WIFI_GOT_IP_TIMEOUT_MS);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Start 'wifi_sta': connected");
    return ESP_OK;
}

esp_err_t wifi_sta_sync_time(void)
{
    // Skip if the clock already looks valid (e.g. RTC survived a warm reboot)
    time_t now = 0;
    time(&now);
    if (now >= SNTP_MIN_VALID_EPOCH) {
        ESP_LOGI(TAG, "Sync time: clock already set (%lld)", (long long)now);
        return ESP_OK;
    }

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
    esp_err_t ret = esp_netif_sntp_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sync time failed: sntp init %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SNTP_SYNC_TIMEOUT_MS));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sync time failed: no response within %d ms", SNTP_SYNC_TIMEOUT_MS);
        esp_netif_sntp_deinit();
        return ESP_ERR_TIMEOUT;
    }

    time(&now);
    esp_netif_sntp_deinit();

    if (now < SNTP_MIN_VALID_EPOCH) {
        ESP_LOGE(TAG, "Sync time failed: clock still invalid (%lld)", (long long)now);
        return ESP_ERR_TIMEOUT;
    }

    struct tm tm_info;
    gmtime_r(&now, &tm_info);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    ESP_LOGI(TAG, "Sync time: system time set to %s UTC", buf);
    return ESP_OK;
}
