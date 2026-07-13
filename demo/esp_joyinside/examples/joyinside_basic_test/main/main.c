/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "esp_joyinside_bootstrap.h"
#include "esp_joyinside_chat.h"
#include "spiffs_audio.h"

static const char *TAG = "joyinside_basic_test";

#define CHAT_CONNECTED_BIT  BIT0

static EventGroupHandle_t s_chat_events;
static esp_joyinside_chat_handle_t s_chat;

static const char *event_name(esp_joyinside_chat_event_t event)
{
    switch (event) {
        case ESP_JOYINSIDE_CHAT_EVENT_CONNECTED: return "CONNECTED";
        case ESP_JOYINSIDE_CHAT_EVENT_DISCONNECTED: return "DISCONNECTED";
        case ESP_JOYINSIDE_CHAT_EVENT_ASR_UPDATE: return "ASR_UPDATE";
        case ESP_JOYINSIDE_CHAT_EVENT_ASR_FINAL: return "ASR_FINAL";
        case ESP_JOYINSIDE_CHAT_EVENT_TTS_SENTENCE_START: return "TTS_SENTENCE_START";
        case ESP_JOYINSIDE_CHAT_EVENT_TTS_COMPLETE: return "TTS_COMPLETE";
        case ESP_JOYINSIDE_CHAT_EVENT_INTERRUPT: return "INTERRUPT";
        case ESP_JOYINSIDE_CHAT_EVENT_CALL_AGENT_START: return "CALL_AGENT_START";
        case ESP_JOYINSIDE_CHAT_EVENT_CALL_AGENT_INTERRUPTED: return "CALL_AGENT_INTERRUPTED";
        case ESP_JOYINSIDE_CHAT_EVENT_CALL_INTENT_END: return "CALL_INTENT_END";
        case ESP_JOYINSIDE_CHAT_EVENT_VOICE_CHAT_EXIT: return "VOICE_CHAT_EXIT";
        case ESP_JOYINSIDE_CHAT_EVENT_ERROR: return "ERROR";
        case ESP_JOYINSIDE_CHAT_EVENT_CUSTOM_EVENT: return "CUSTOM_EVENT";
        case ESP_JOYINSIDE_CHAT_EVENT_WS_EVENT: return "WS_EVENT";
        case ESP_JOYINSIDE_CHAT_EVENT_AUDIO_DATA: return "AUDIO_DATA";
        default: return "UNKNOWN";
    }
}

static void chat_event_callback(esp_joyinside_chat_event_t event, void *event_data, void *ctx)
{
    (void)ctx;
    if (event == ESP_JOYINSIDE_CHAT_EVENT_CONNECTED) {
        xEventGroupSetBits(s_chat_events, CHAT_CONNECTED_BIT);
    }
    if (event == ESP_JOYINSIDE_CHAT_EVENT_ASR_UPDATE ||
        event == ESP_JOYINSIDE_CHAT_EVENT_ASR_FINAL ||
        event == ESP_JOYINSIDE_CHAT_EVENT_TTS_SENTENCE_START ||
        event == ESP_JOYINSIDE_CHAT_EVENT_ERROR ||
        event == ESP_JOYINSIDE_CHAT_EVENT_CUSTOM_EVENT) {
        ESP_LOGI(TAG, "event=%s data=%s", event_name(event),
                 event_data ? (const char *)event_data : "");
        return;
    }
    if (event == ESP_JOYINSIDE_CHAT_EVENT_AUDIO_DATA && event_data != NULL) {
        const esp_joyinside_chat_audio_data_t *audio = event_data;
        ESP_LOGI(TAG, "event=AUDIO_DATA len=%d", audio->len);
        return;
    }
    ESP_LOGI(TAG, "event=%s", event_name(event));
}

static void chat_audio_callback(const uint8_t *data, int len, void *ctx)
{
    (void)data;
    (void)ctx;
    ESP_LOGI(TAG, "downlink audio chunk len=%d", len);
}

static esp_err_t run_bootstrap_did_test(void)
{
#if CONFIG_JOYINSIDE_TEST_RUN_BOOTSTRAP_DID
    char *device_id = NULL;
    ESP_RETURN_ON_ERROR(esp_joyinside_bootstrap_generate_device_id(ESP_MAC_WIFI_STA, &device_id),
                        TAG, "generate device ID");
    ESP_LOGI(TAG, "[PASS] device ID generated, len=%d", (int)strlen(device_id));
    free(device_id);
    return ESP_OK;
#else
    ESP_LOGI(TAG, "[SKIP] bootstrap DID test");
    return ESP_OK;
#endif
}

static esp_err_t sync_system_time(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&config), TAG, "SNTP init failed");

    for (int retry = 0; retry < 15; ++retry) {
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_OK) {
            struct timeval now = {0};
            gettimeofday(&now, NULL);
            ESP_LOGI(TAG, "system time synced, tv_sec=%ld", (long)now.tv_sec);
            return ESP_OK;
        }
        ESP_LOGI(TAG, "waiting for SNTP... (%d/15)", retry + 1);
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t resolve_bot_id(const char *device_id, char **out_bot_id)
{
    if (CONFIG_JOYINSIDE_BOT_ID[0] != '\0') {
        *out_bot_id = strdup(CONFIG_JOYINSIDE_BOT_ID);
        return *out_bot_id ? ESP_OK : ESP_ERR_NO_MEM;
    }

#if CONFIG_JOYINSIDE_TEST_RUN_BOOTSTRAP_REGISTER
        esp_joyinside_bootstrap_config_t bootstrap = {
            .struct_version = ESP_JOYINSIDE_BOOTSTRAP_CONFIG_VERSION,
            .access_key_id = CONFIG_JOYINSIDE_ACCESS_KEY_ID,
        .access_key_secret = CONFIG_JOYINSIDE_ACCESS_KEY_SECRET,
        .vendor_id = CONFIG_JOYINSIDE_VENDOR_ID,
        .app_id = CONFIG_JOYINSIDE_APP_ID,
        .device_model = CONFIG_JOYINSIDE_DEVICE_MODEL,
        .device_id = device_id,
    };
    ESP_RETURN_ON_ERROR(esp_joyinside_bootstrap_register(&bootstrap, out_bot_id),
                        TAG, "bootstrap register failed");
    ESP_LOGI(TAG, "[PASS] bootstrap register");
    return ESP_OK;
#else
    ESP_LOGE(TAG, "bot ID is empty and bootstrap register test is disabled");
    return ESP_ERR_INVALID_STATE;
#endif
}

static esp_err_t run_chat_connect_test(const char *device_id, const char *bot_id)
{
#if CONFIG_JOYINSIDE_TEST_RUN_CHAT_CONNECT
    s_chat_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_chat_events != NULL, ESP_ERR_NO_MEM, TAG, "event group alloc failed");

    esp_joyinside_chat_config_t config = ESP_JOYINSIDE_CHAT_DEFAULT_CONFIG();
    config.bot_id = (char *)bot_id;
    config.access_key_id = CONFIG_JOYINSIDE_ACCESS_KEY_ID;
    config.access_key_secret = CONFIG_JOYINSIDE_ACCESS_KEY_SECRET;
    config.device_id = (char *)device_id;
    config.device_token = CONFIG_JOYINSIDE_DEVICE_TOKEN[0] ? CONFIG_JOYINSIDE_DEVICE_TOKEN : NULL;
    config.mcp_label = CONFIG_JOYINSIDE_DEVICE_MODEL;
    config.uplink_audio_type = ESP_JOYINSIDE_AUDIO_TYPE_PCM;
    config.downlink_audio_type = ESP_JOYINSIDE_AUDIO_TYPE_PCM;
    config.sample_rate = 16000;
    config.frame_size_ms = 20;
    config.event_callback = chat_event_callback;
    config.audio_callback = chat_audio_callback;

    ESP_RETURN_ON_ERROR(esp_joyinside_chat_init(&config, &s_chat), TAG, "chat init failed");
    ESP_RETURN_ON_ERROR(esp_joyinside_chat_start(s_chat), TAG, "chat start failed");

    EventBits_t bits = xEventGroupWaitBits(s_chat_events, CHAT_CONNECTED_BIT, pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(30000));
    ESP_RETURN_ON_FALSE(bits & CHAT_CONNECTED_BIT, ESP_ERR_TIMEOUT, TAG, "chat connect timeout");
    ESP_LOGI(TAG, "[PASS] chat connected and init sent");

#if CONFIG_JOYINSIDE_TEST_SEND_SPIFFS_PCM
    ESP_RETURN_ON_ERROR(spiffs_audio_send_pcm_file(s_chat,
                                                   CONFIG_JOYINSIDE_TEST_SPIFFS_PCM_PATH,
                                                   CONFIG_JOYINSIDE_TEST_PCM_FRAME_BYTES,
                                                   CONFIG_JOYINSIDE_TEST_PCM_FRAME_INTERVAL_MS),
                        TAG, "send SPIFFS PCM failed");
#elif CONFIG_JOYINSIDE_TEST_SEND_SILENCE
    uint8_t silence[640] = {0};
    for (int i = 0; i < 5; ++i) {
        esp_err_t err = esp_joyinside_chat_send_audio_data(s_chat, silence, sizeof(silence));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "send silence frame %d: %s", i, esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ESP_LOGI(TAG, "sent %d silence PCM frames", 5);
#endif

    vTaskDelay(pdMS_TO_TICKS(CONFIG_JOYINSIDE_TEST_CHAT_DURATION_SEC * 1000));
    ESP_RETURN_ON_ERROR(esp_joyinside_chat_send_audio_complete(s_chat), TAG, "audio complete failed");
    ESP_RETURN_ON_ERROR(esp_joyinside_chat_stop(s_chat), TAG, "chat stop failed");
    ESP_RETURN_ON_ERROR(esp_joyinside_chat_deinit(s_chat), TAG, "chat deinit failed");
    s_chat = NULL;
    vEventGroupDelete(s_chat_events);
    s_chat_events = NULL;
    ESP_LOGI(TAG, "[PASS] chat session closed cleanly");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "[SKIP] chat connect test");
    (void)device_id;
    (void)bot_id;
    return ESP_OK;
#endif
}

static esp_err_t run_network_tests(void)
{
#if CONFIG_JOYINSIDE_TEST_ENABLE_NETWORK
    char *device_id = NULL;
    char *bot_id = NULL;
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_ERROR(esp_joyinside_bootstrap_generate_device_id(ESP_MAC_WIFI_STA, &device_id),
                        TAG, "generate device ID");
    ESP_RETURN_ON_ERROR(sync_system_time(), TAG, "SNTP sync failed");
    ESP_GOTO_ON_ERROR(resolve_bot_id(device_id, &bot_id), cleanup, TAG, "resolve bot ID failed");
    ESP_GOTO_ON_ERROR(run_chat_connect_test(device_id, bot_id), cleanup, TAG, "chat test failed");

cleanup:
    free(bot_id);
    free(device_id);
    return ret;
#else
    ESP_LOGI(TAG, "[SKIP] network-dependent tests");
    return ESP_OK;
#endif
}

void app_main(void)
{
    esp_err_t err = ESP_OK;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "=== JoyInside basic test start ===");

    if (run_bootstrap_did_test() != ESP_OK) {
        err = ESP_FAIL;
    }

#if CONFIG_JOYINSIDE_TEST_ENABLE_NETWORK
    ESP_ERROR_CHECK(example_connect());
#if CONFIG_JOYINSIDE_TEST_SEND_SPIFFS_PCM
    ESP_ERROR_CHECK(spiffs_audio_mount());
#endif
    if (run_network_tests() != ESP_OK) {
        err = ESP_FAIL;
    }
#if CONFIG_JOYINSIDE_TEST_SEND_SPIFFS_PCM
    spiffs_audio_unmount();
#endif
#endif

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "=== JoyInside basic test PASSED ===");
    } else {
        ESP_LOGE(TAG, "=== JoyInside basic test FAILED ===");
    }
}
