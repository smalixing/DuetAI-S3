/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_check.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "joyinside_auth.h"
#include "joyinside_envelope.h"
#include "esp_joyinside_chat_priv.h"

static const char *TAG = ESP_JOYINSIDE_TAG;

static char *sdk_strdup(const char *s)
{
    return s ? strdup(s) : NULL;
}

static void destroy_chat(joyinside_chat_t *chat)
{
    if (chat == NULL) {
        return;
    }
    if (chat->ws) {
        if (chat->started) {
            esp_websocket_client_stop(chat->ws);
        }
        esp_websocket_client_destroy(chat->ws);
    }
    if (chat->event_group) {
        vEventGroupDelete(chat->event_group);
    }
    if (chat->lock) {
        vSemaphoreDelete(chat->lock);
    }
    free(chat->ws_base_url);
    free(chat->bot_id);
    free(chat->access_key_id);
    free(chat->access_key_secret);
    free(chat->device_id);
    free(chat->device_token);
    free(chat->mcp_label);
    free(chat->recv_buf);
    free(chat);
}

void esp_joyinside_post_event(joyinside_chat_t *chat, int32_t event_id)
{
    if (chat && chat->config.event_callback) {
        chat->config.event_callback((esp_joyinside_chat_event_t)event_id, NULL,
                                    chat->config.event_callback_ctx);
    }
}

void esp_joyinside_post_event_text(joyinside_chat_t *chat, int32_t event_id, const char *text)
{
    if (chat && text && chat->config.event_callback) {
        chat->config.event_callback((esp_joyinside_chat_event_t)event_id, (void *)text,
                                    chat->config.event_callback_ctx);
    }
}

void esp_joyinside_post_event_blob(joyinside_chat_t *chat, int32_t event_id,
                                   const void *data, size_t size)
{
    if (chat == NULL || data == NULL || size == 0) {
        return;
    }

    if (event_id == ESP_JOYINSIDE_CHAT_EVENT_AUDIO_DATA &&
        size >= sizeof(esp_joyinside_chat_audio_data_t)) {
        const esp_joyinside_chat_audio_data_t *audio = (const esp_joyinside_chat_audio_data_t *)data;
        if (chat->config.audio_callback != NULL) {
            if (audio->len > 0) {
                chat->config.audio_callback((const uint8_t *)audio->data, audio->len,
                                            chat->config.audio_callback_ctx);
            }
            return;
        }
    }

    if (chat->config.event_callback != NULL) {
        chat->config.event_callback((esp_joyinside_chat_event_t)event_id, (void *)data,
                                    chat->config.event_callback_ctx);
    }
}

esp_err_t esp_joyinside_chat_init(const esp_joyinside_chat_config_t *config,
                                  esp_joyinside_chat_handle_t *chat_hd)
{
    ESP_RETURN_ON_FALSE(config != NULL && chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid args");
    if (config->struct_version != 0 &&
        config->struct_version != ESP_JOYINSIDE_CHAT_CONFIG_VERSION) {
        ESP_LOGE(TAG, "Init failed: unsupported config version %lu",
                 (unsigned long)config->struct_version);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_FALSE(config->ws_base_url != NULL && config->bot_id != NULL &&
                        config->access_key_id != NULL && config->access_key_secret != NULL &&
                        config->device_id != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "required config fields missing");

    joyinside_chat_t *chat = (joyinside_chat_t *)calloc(1, sizeof(*chat));
    ESP_RETURN_ON_FALSE(chat != NULL, ESP_ERR_NO_MEM, TAG, "no mem chat");

    memcpy(&chat->config, config, sizeof(*config));
    if (chat->config.websocket_connect_timeout <= 0) {
        chat->config.websocket_connect_timeout = CONFIG_ESP_JOYINSIDE_WS_CONNECT_TIMEOUT_MS;
    }
    chat->config.ws_base_url = NULL;
    chat->config.bot_id = NULL;
    chat->config.access_key_id = NULL;
    chat->config.access_key_secret = NULL;
    chat->config.device_id = NULL;
    chat->config.device_token = NULL;
    chat->config.mcp_label = NULL;

    chat->ws_base_url = sdk_strdup(config->ws_base_url);
    chat->bot_id = sdk_strdup(config->bot_id);
    chat->access_key_id = sdk_strdup(config->access_key_id);
    chat->access_key_secret = sdk_strdup(config->access_key_secret);
    chat->device_id = sdk_strdup(config->device_id);
    chat->device_token = sdk_strdup(config->device_token);
    chat->mcp_label = sdk_strdup(config->mcp_label ? config->mcp_label : "esp-joyinside");

    if (chat->ws_base_url == NULL || chat->bot_id == NULL || chat->access_key_id == NULL ||
        chat->access_key_secret == NULL || chat->device_id == NULL ||
        chat->mcp_label == NULL) {
        destroy_chat(chat);
        return ESP_ERR_NO_MEM;
    }

    chat->lock = xSemaphoreCreateMutex();
    if (chat->lock == NULL) {
        destroy_chat(chat);
        return ESP_ERR_NO_MEM;
    }

    chat->event_group = xEventGroupCreate();
    if (chat->event_group == NULL) {
        destroy_chat(chat);
        return ESP_ERR_NO_MEM;
    }

    esp_websocket_client_config_t ws_cfg = {
        .uri = chat->ws_base_url,
        .buffer_size = CONFIG_ESP_JOYINSIDE_WS_BUFFER_SIZE,
        .reconnect_timeout_ms = CONFIG_ESP_JOYINSIDE_WS_RECONNECT_TIMEOUT_MS,
        .network_timeout_ms = CONFIG_ESP_JOYINSIDE_WS_NETWORK_TIMEOUT_MS,
        .task_prio = CONFIG_ESP_JOYINSIDE_WS_TASK_PRIO,
        .task_stack = CONFIG_ESP_JOYINSIDE_WS_TASK_STACK,
        .task_name = CONFIG_ESP_JOYINSIDE_WS_TASK_NAME[0] ? CONFIG_ESP_JOYINSIDE_WS_TASK_NAME : NULL,
#if CONFIG_ESP_JOYINSIDE_WS_TASK_CORE_ID_SET
        .task_core_id_set = true,
        .task_core_id = CONFIG_ESP_JOYINSIDE_WS_TASK_CORE_ID,
#else
        .task_core_id_set = false,
        .task_core_id = 0,
#endif
        .disable_auto_reconnect = true,
    };
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    ws_cfg.crt_bundle_attach = esp_crt_bundle_attach;
#endif

    chat->ws = esp_websocket_client_init(&ws_cfg);
    if (chat->ws == NULL) {
        destroy_chat(chat);
        return ESP_FAIL;
    }

    esp_err_t err = esp_websocket_register_events(chat->ws, WEBSOCKET_EVENT_ANY,
                                                  esp_joyinside_chat_ws_event_handler, chat);
    if (err != ESP_OK) {
        destroy_chat(chat);
        return err;
    }

    if (joyinside_env_gen_event_id(chat->mid, sizeof(chat->mid)) != ESP_OK) {
        destroy_chat(chat);
        return ESP_FAIL;
    }

    *chat_hd = chat;
    return ESP_OK;
}

esp_err_t esp_joyinside_chat_deinit(esp_joyinside_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    destroy_chat((joyinside_chat_t *)chat_hd);
    return ESP_OK;
}

esp_err_t esp_joyinside_chat_start(esp_joyinside_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    joyinside_chat_t *chat = (joyinside_chat_t *)chat_hd;

    char *uri = joyinside_auth_generate_uri(chat->ws_base_url, chat->access_key_id,
                                            chat->access_key_secret, chat->bot_id);
    ESP_RETURN_ON_FALSE(uri != NULL, ESP_ERR_NO_MEM, TAG, "failed to build auth uri");

    esp_err_t err = esp_websocket_client_set_uri(chat->ws, uri);
    free(uri);
    ESP_RETURN_ON_ERROR(err, TAG, "set uri failed");

    xEventGroupClearBits(chat->event_group, ESP_JOYINSIDE_WS_CONNECTED_BIT);
    ESP_RETURN_ON_ERROR(esp_websocket_client_start(chat->ws), TAG, "websocket start failed");
    chat->started = true;

    EventBits_t bits = xEventGroupWaitBits(chat->event_group, ESP_JOYINSIDE_WS_CONNECTED_BIT,
                                           pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(chat->config.websocket_connect_timeout));
    err = (bits & ESP_JOYINSIDE_WS_CONNECTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
    if (err != ESP_OK) {
        esp_websocket_client_stop(chat->ws);
        chat->started = false;
        return err;
    }

    return esp_joyinside_proto_send_init(chat);
}

esp_err_t esp_joyinside_chat_stop(esp_joyinside_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    joyinside_chat_t *chat = (joyinside_chat_t *)chat_hd;
    if (chat->started) {
        esp_websocket_client_stop(chat->ws);
        chat->started = false;
    }
    return ESP_OK;
}

bool esp_joyinside_chat_is_connected(esp_joyinside_chat_handle_t chat_hd)
{
    joyinside_chat_t *chat = (joyinside_chat_t *)chat_hd;
    return (chat && chat->ws) ? esp_websocket_client_is_connected(chat->ws) : false;
}

esp_err_t esp_joyinside_chat_send_audio_data(esp_joyinside_chat_handle_t chat_hd,
                                             const uint8_t *data,
                                             size_t len)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL && data != NULL && len > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid audio args");
    joyinside_chat_t *chat = (joyinside_chat_t *)chat_hd;
    ESP_RETURN_ON_FALSE(esp_websocket_client_is_connected(chat->ws),
                        ESP_ERR_INVALID_STATE, TAG, "ws not connected");

    return joyinside_env_ws_send_bin_retry(chat->ws, data, (int)len, 1, 0,
                                           ESP_JOYINSIDE_AUDIO_SEND_TO_MS,
                                           "joyinside.audio.bin");
}

esp_err_t esp_joyinside_chat_send_audio_complete(esp_joyinside_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    return esp_joyinside_proto_send_audio_finish((joyinside_chat_t *)chat_hd);
}

esp_err_t esp_joyinside_chat_interrupt(esp_joyinside_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    return esp_joyinside_proto_send_interrupt((joyinside_chat_t *)chat_hd);
}
