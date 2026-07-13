/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"

#include "mbedtls/base64.h"

#include "joyinside_envelope.h"

static const char *TAG = "JOYINSIDE_ENV";

static void fill_hex(char *out, size_t count)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < count; i++) {
        out[i] = hex[esp_random() & 0x0f];
    }
}

esp_err_t joyinside_env_gen_event_id(char *out, size_t out_size)
{
    ESP_RETURN_ON_FALSE(out != NULL && out_size >= JOYINSIDE_EVENT_ID_BUF_LEN,
                        ESP_ERR_INVALID_ARG, TAG, "invalid event id buffer");

    fill_hex(out, 8);
    out[8] = '-';
    fill_hex(out + 9, 4);
    out[13] = '-';
    out[14] = '4';
    fill_hex(out + 15, 3);
    out[18] = '-';
    out[19] = "89ab"[esp_random() & 0x03];
    fill_hex(out + 20, 3);
    out[23] = '-';
    fill_hex(out + 24, 12);
    out[36] = '\0';
    return ESP_OK;
}

char *joyinside_env_make_simple_event(const char *mid, const char *event_type)
{
    if (mid == NULL || event_type == NULL) {
        return NULL;
    }

    const char *fmt =
        "{\"mid\":\"%s\",\"contentType\":\"EVENT\","
        "\"content\":{\"eventType\":\"%s\"}}";
    int need = snprintf(NULL, 0, fmt, mid, event_type);
    if (need <= 0) {
        return NULL;
    }
    char *json = (char *)calloc(1, (size_t)need + 1);
    if (json == NULL) {
        return NULL;
    }
    snprintf(json, (size_t)need + 1, fmt, mid, event_type);
    return json;
}

char *joyinside_env_make_typed_json(const char *mid, const char *content_type, const char *content_json)
{
    if (mid == NULL || content_type == NULL || content_json == NULL) {
        return NULL;
    }

    const char *fmt = "{\"mid\":\"%s\",\"contentType\":\"%s\",\"content\":%s}";
    int need = snprintf(NULL, 0, fmt, mid, content_type, content_json);
    if (need <= 0) {
        return NULL;
    }
    char *json = (char *)calloc(1, (size_t)need + 1);
    if (json == NULL) {
        return NULL;
    }
    snprintf(json, (size_t)need + 1, fmt, mid, content_type, content_json);
    return json;
}

static esp_err_t ws_send_with_retry(int (*send_fn)(esp_websocket_client_handle_t, const char *, int, TickType_t),
                                    esp_websocket_client_handle_t ws,
                                    const uint8_t *data,
                                    int len,
                                    int retries,
                                    int delay_ms,
                                    int timeout_ms,
                                    const char *op_name)
{
    ESP_RETURN_ON_FALSE(ws != NULL && data != NULL && len > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid ws send args");
    if (retries < 1) {
        retries = 1;
    }
    if (delay_ms < 0) {
        delay_ms = 0;
    }
    if (timeout_ms <= 0) {
        timeout_ms = 5000;
    }

    for (int i = 0; i < retries; i++) {
        int sent = send_fn(ws, (const char *)data, len, pdMS_TO_TICKS(timeout_ms));
        if (sent == len) {
            return ESP_OK;
        }
        if (sent >= 0) {
            ESP_LOGW(TAG, "%s partial send: %d/%d", op_name ? op_name : "ws_send", sent, len);
        } else {
            ESP_LOGW(TAG, "%s failed on attempt %d", op_name ? op_name : "ws_send", i + 1);
        }
        if (i + 1 < retries && delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    return ESP_FAIL;
}

esp_err_t joyinside_env_ws_send_text_retry(esp_websocket_client_handle_t ws,
                                           const char *json,
                                           int len,
                                           int retries,
                                           int delay_ms,
                                           int timeout_ms,
                                           const char *op_name)
{
    return ws_send_with_retry(esp_websocket_client_send_text, ws,
                              (const uint8_t *)json, len, retries, delay_ms,
                              timeout_ms, op_name);
}

esp_err_t joyinside_env_ws_send_bin_retry(esp_websocket_client_handle_t ws,
                                          const uint8_t *data,
                                          int len,
                                          int retries,
                                          int delay_ms,
                                          int timeout_ms,
                                          const char *op_name)
{
    return ws_send_with_retry(esp_websocket_client_send_bin, ws,
                              data, len, retries, delay_ms,
                              timeout_ms, op_name);
}

esp_err_t joyinside_env_b64_decode_alloc(const char *b64, size_t b64_len,
                                         uint8_t **out_buf, size_t *out_len)
{
    ESP_RETURN_ON_FALSE(b64 != NULL && out_buf != NULL && out_len != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid b64 args");

    size_t cap = ((b64_len + 3) / 4) * 3 + 1;
    uint8_t *buf = (uint8_t *)malloc(cap);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "no mem for b64 decode");

    size_t used = 0;
    int ret = mbedtls_base64_decode(buf, cap, &used,
                                    (const unsigned char *)b64, b64_len);
    if (ret != 0) {
        free(buf);
        return ESP_FAIL;
    }

    *out_buf = buf;
    *out_len = used;
    return ESP_OK;
}
