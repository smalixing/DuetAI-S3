/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_websocket_client.h"

#include "esp_joyinside_chat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_JOYINSIDE_TAG  "ESP_JOYINSIDE_CHAT"
#define ESP_JOYINSIDE_AUDIO_SEND_TO_MS  CONFIG_ESP_JOYINSIDE_WS_AUDIO_SEND_TIMEOUT_MS
#define ESP_JOYINSIDE_WS_CONNECTED_BIT  (1u << 0)

#define ESP_JOYINSIDE_LOCK(chat)    xSemaphoreTake((chat)->lock, portMAX_DELAY)
#define ESP_JOYINSIDE_UNLOCK(chat)  xSemaphoreGive((chat)->lock)

typedef struct joyinside_chat {
    esp_websocket_client_handle_t ws;
    EventGroupHandle_t            event_group;
    esp_joyinside_chat_config_t   config;
    char                         *ws_base_url;
    char                         *bot_id;
    char                         *access_key_id;
    char                         *access_key_secret;
    char                         *device_id;
    char                         *device_token;
    char                         *mcp_label;
    char                          mid[JOYINSIDE_EVENT_ID_BUF_LEN];
    char                         *recv_buf;
    SemaphoreHandle_t             lock;
    bool                          started;
} joyinside_chat_t;

esp_err_t esp_joyinside_proto_send_init(joyinside_chat_t *chat);
esp_err_t esp_joyinside_proto_send_audio_finish(joyinside_chat_t *chat);
esp_err_t esp_joyinside_proto_send_interrupt(joyinside_chat_t *chat);

void esp_joyinside_post_event(joyinside_chat_t *chat, int32_t event_id);
void esp_joyinside_post_event_text(joyinside_chat_t *chat, int32_t event_id, const char *text);
void esp_joyinside_post_event_blob(joyinside_chat_t *chat, int32_t event_id,
                                   const void *data, size_t size);

void esp_joyinside_chat_ws_event_handler(void *handler_args,
                                         esp_event_base_t base,
                                         int32_t event_id,
                                         void *event_data);

#ifdef __cplusplus
}
#endif
