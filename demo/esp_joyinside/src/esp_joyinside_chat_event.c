/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_websocket_client.h"

#include "cJSON.h"

#include "joyinside_envelope.h"
#include "esp_joyinside_chat_priv.h"

static const char *TAG = "ESP_JOYINSIDE_EVENT";

static void on_event_type(joyinside_chat_t *chat, const char *event_type, cJSON *content)
{
    cJSON *event_data = cJSON_GetObjectItem(content, "eventData");

    if (strcmp(event_type, "TTS_COMPLETE") == 0) {
        esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_TTS_COMPLETE);
    } else if (strcmp(event_type, "CALL_AGENT_INTERRUPTED") == 0) {
        esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_CALL_AGENT_INTERRUPTED);
    } else if (strcmp(event_type, "VOICE_CHAT_EXIT_EVENT") == 0) {
        esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_VOICE_CHAT_EXIT);
    } else if (strcmp(event_type, "CALL_INTENT_END_EVENT") == 0) {
        esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_CALL_INTENT_END);
    } else if (strcmp(event_type, "INTERRUPT") == 0) {
        esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_INTERRUPT);
    } else if (strcmp(event_type, "CALL_AGENT_START_EVENT") == 0) {
        esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_CALL_AGENT_START);
    } else if (strcmp(event_type, "TTS_SENTENCE_START") == 0) {
        cJSON *text = event_data ? cJSON_GetObjectItem(event_data, "text") : NULL;
        if (cJSON_IsString(text) && text->valuestring != NULL) {
            esp_joyinside_post_event_text(chat, ESP_JOYINSIDE_CHAT_EVENT_TTS_SENTENCE_START,
                                          text->valuestring);
        }
    } else {
        char *json = cJSON_PrintUnformatted(content);
        if (json != NULL) {
            esp_joyinside_post_event_text(chat, ESP_JOYINSIDE_CHAT_EVENT_CUSTOM_EVENT, json);
            free(json);
        }
    }
}

static void on_content_type(joyinside_chat_t *chat, const char *content_type, cJSON *content)
{
    if (strcmp(content_type, "ASR") == 0) {
        cJSON *text = cJSON_GetObjectItem(content, "text");
        cJSON *text_type = cJSON_GetObjectItem(content, "textType");
        if (cJSON_IsString(text) && text->valuestring != NULL) {
            if (cJSON_IsString(text_type) && text_type->valuestring != NULL &&
                strcmp(text_type->valuestring, "IS_FINAL") == 0) {
                esp_joyinside_post_event_text(chat, ESP_JOYINSIDE_CHAT_EVENT_ASR_FINAL,
                                              text->valuestring);
            } else {
                esp_joyinside_post_event_text(chat, ESP_JOYINSIDE_CHAT_EVENT_ASR_UPDATE,
                                              text->valuestring);
            }
        }
    } else if (strcmp(content_type, "EVENT") == 0) {
        cJSON *event_type = cJSON_GetObjectItem(content, "eventType");
        if (cJSON_IsString(event_type) && event_type->valuestring != NULL) {
            on_event_type(chat, event_type->valuestring, content);
        }
    } else if (strcmp(content_type, "TTS") == 0) {
        cJSON *audio_b64 = cJSON_GetObjectItem(content, "audioBase64");
        if (cJSON_IsString(audio_b64) && audio_b64->valuestring != NULL) {
            uint8_t *decoded = NULL;
            size_t decoded_len = 0;
            if (joyinside_env_b64_decode_alloc(audio_b64->valuestring,
                                               strlen(audio_b64->valuestring),
                                               &decoded, &decoded_len) == ESP_OK &&
                decoded_len > 0) {
                size_t total = sizeof(esp_joyinside_chat_audio_data_t) + decoded_len;
                esp_joyinside_chat_audio_data_t *evt =
                    (esp_joyinside_chat_audio_data_t *)malloc(total);
                if (evt != NULL) {
                    evt->len = (int)decoded_len;
                    memcpy(evt->data, decoded, decoded_len);
                    esp_joyinside_post_event_blob(chat, ESP_JOYINSIDE_CHAT_EVENT_AUDIO_DATA,
                                                  evt, total);
                    free(evt);
                }
                free(decoded);
            }
        }
    }
}

static void on_text_frame(joyinside_chat_t *chat, const char *raw)
{
    cJSON *root = cJSON_Parse(raw);
    if (root == NULL) {
        return;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (cJSON_IsNumber(code) && code->valueint != 200) {
        char *json = cJSON_PrintUnformatted(root);
        if (json != NULL) {
            esp_joyinside_post_event_text(chat, ESP_JOYINSIDE_CHAT_EVENT_ERROR, json);
            free(json);
        }
        cJSON_Delete(root);
        return;
    }

    cJSON *content_type = cJSON_GetObjectItem(root, "contentType");
    cJSON *content = cJSON_GetObjectItem(root, "content");
    if (cJSON_IsString(content_type) && content_type->valuestring != NULL &&
        cJSON_IsObject(content)) {
        on_content_type(chat, content_type->valuestring, content);
    }

    cJSON_Delete(root);
}

static void on_binary_frame(joyinside_chat_t *chat, const uint8_t *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }

    size_t total = sizeof(esp_joyinside_chat_audio_data_t) + (size_t)len;
    esp_joyinside_chat_audio_data_t *evt_audio =
        (esp_joyinside_chat_audio_data_t *)malloc(total);
    if (evt_audio != NULL) {
        evt_audio->len = len;
        memcpy(evt_audio->data, data, (size_t)len);
        esp_joyinside_post_event_blob(chat, ESP_JOYINSIDE_CHAT_EVENT_AUDIO_DATA,
                                      evt_audio, total);
        free(evt_audio);
    }
}

static void on_ws_data(joyinside_chat_t *chat, esp_websocket_event_data_t *data)
{
    if (data == NULL) {
        return;
    }

    if (data->op_code == 0x01) {
        if (data->payload_offset == 0) {
            if (data->payload_len <= 0 ||
                data->payload_len > CONFIG_ESP_JOYINSIDE_WS_MAX_TEXT_FRAME_SIZE) {
                ESP_LOGW(TAG, "text frame exceeds configured limit: %d", data->payload_len);
                esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_ERROR);
                return;
            }
            free(chat->recv_buf);
            chat->recv_buf = (char *)calloc(1, (size_t)data->payload_len + 1);
            if (chat->recv_buf == NULL) {
                ESP_LOGE(TAG, "no mem for text frame");
                esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_ERROR);
                return;
            }
        }

        if (chat->recv_buf == NULL || data->payload_offset < 0 || data->data_len < 0 ||
            data->payload_len < 0 ||
            data->payload_offset + data->data_len > data->payload_len) {
            ESP_LOGE(TAG, "invalid text frame fragment");
            free(chat->recv_buf);
            chat->recv_buf = NULL;
            esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_ERROR);
            return;
        }

        memcpy(chat->recv_buf + data->payload_offset, data->data_ptr, (size_t)data->data_len);
        if (data->payload_offset + data->data_len == data->payload_len) {
            on_text_frame(chat, chat->recv_buf);
            free(chat->recv_buf);
            chat->recv_buf = NULL;
        }
    } else if (data->op_code == 0x02) {
        on_binary_frame(chat, (const uint8_t *)data->data_ptr, data->data_len);
    }
}

void esp_joyinside_chat_ws_event_handler(void *handler_args,
                                         esp_event_base_t base,
                                         int32_t event_id,
                                         void *event_data)
{
    (void)base;
    joyinside_chat_t *chat = (joyinside_chat_t *)handler_args;
    if (chat == NULL) {
        return;
    }

    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    esp_joyinside_ws_event_t we = {
        .handle = chat->ws,
        .event_id = (esp_websocket_event_id_t)event_id,
    };
    esp_joyinside_post_event_blob(chat, ESP_JOYINSIDE_CHAT_EVENT_WS_EVENT, &we, sizeof(we));

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            xEventGroupSetBits(chat->event_group, ESP_JOYINSIDE_WS_CONNECTED_BIT);
            esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_CONNECTED);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            free(chat->recv_buf);
            chat->recv_buf = NULL;
            xEventGroupClearBits(chat->event_group, ESP_JOYINSIDE_WS_CONNECTED_BIT);
            esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_DISCONNECTED);
            break;

        case WEBSOCKET_EVENT_DATA:
            on_ws_data(chat, data);
            break;

        case WEBSOCKET_EVENT_ERROR:
            esp_joyinside_post_event(chat, ESP_JOYINSIDE_CHAT_EVENT_ERROR);
            break;

        default:
            break;
    }
}
