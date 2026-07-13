/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_check.h"
#include "esp_log.h"

#include "cJSON.h"

#include "joyinside_jwt.h"
#include "joyinside_envelope.h"
#include "esp_joyinside_chat_priv.h"

static const char *TAG = ESP_JOYINSIDE_TAG;

#define WS_SEND_RETRY_DEFAULT   3
#define WS_SEND_RETRY_DELAY_MS  50
#define WS_SEND_TIMEOUT_MS      (5 * 1000)

static uint64_t now_timestamp_sec(void)
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec;
}

static const char *codec_name(esp_joyinside_audio_type_t type)
{
    return type == ESP_JOYINSIDE_AUDIO_TYPE_OPUS ? "opus" : "pcm";
}

static esp_err_t send_json_text(joyinside_chat_t *chat, const char *json, const char *op_name)
{
    return joyinside_env_ws_send_text_retry(chat->ws, json, (int)strlen(json),
                                            WS_SEND_RETRY_DEFAULT,
                                            WS_SEND_RETRY_DELAY_MS,
                                            WS_SEND_TIMEOUT_MS, op_name);
}

esp_err_t esp_joyinside_proto_send_init(joyinside_chat_t *chat)
{
    ESP_RETURN_ON_FALSE(chat != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid chat");

    ESP_JOYINSIDE_LOCK(chat);
    if (joyinside_env_gen_event_id(chat->mid, sizeof(chat->mid)) != ESP_OK) {
        ESP_JOYINSIDE_UNLOCK(chat);
        return ESP_FAIL;
    }

    char *jwt = NULL;
    if (chat->device_token != NULL && chat->device_token[0] != '\0') {
        jwt = joyinside_jwt_create(chat->device_id, chat->device_token, now_timestamp_sec());
        if (jwt == NULL) {
            ESP_JOYINSIDE_UNLOCK(chat);
            return ESP_FAIL;
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *event_data = cJSON_CreateObject();
    cJSON *audio = cJSON_CreateObject();
    cJSON *output = cJSON_CreateObject();
    cJSON *input = cJSON_CreateObject();
    cJSON *timbre = cJSON_CreateObject();
    cJSON *features = cJSON_CreateObject();
    cJSON *mcp = cJSON_CreateObject();
    cJSON *server_info = cJSON_CreateObject();
    cJSON *properties = cJSON_CreateObject();
    char sample_rate_str[16];
    char frame_size_str[16];
    char voice_volume_str[16];

    if (root == NULL || event_data == NULL || audio == NULL || output == NULL || input == NULL ||
        timbre == NULL || features == NULL || mcp == NULL ||
        server_info == NULL || properties == NULL) {
        free(jwt);
        cJSON_Delete(root);
        cJSON_Delete(event_data);
        cJSON_Delete(audio);
        cJSON_Delete(output);
        cJSON_Delete(input);
        cJSON_Delete(timbre);
        cJSON_Delete(features);
        cJSON_Delete(mcp);
        cJSON_Delete(server_info);
        cJSON_Delete(properties);
        ESP_JOYINSIDE_UNLOCK(chat);
        return ESP_ERR_NO_MEM;
    }

    snprintf(sample_rate_str, sizeof(sample_rate_str), "%d", chat->config.sample_rate);
    snprintf(frame_size_str, sizeof(frame_size_str), "%d", chat->config.frame_size_ms);
    snprintf(voice_volume_str, sizeof(voice_volume_str), "%d", chat->config.voice_volume);

    cJSON_AddBoolToObject(audio, "binary", true);
    cJSON_AddStringToObject(output, "codec", codec_name(chat->config.downlink_audio_type));
    cJSON_AddStringToObject(output, "frameSizeMs", frame_size_str);
    cJSON_AddStringToObject(output, "sampleRate", sample_rate_str);
    cJSON_AddStringToObject(input, "codec", codec_name(chat->config.uplink_audio_type));
    cJSON_AddStringToObject(input, "sampleRate", sample_rate_str);
    cJSON_AddStringToObject(timbre, "voiceVolume", voice_volume_str);

    cJSON_AddItemToObject(audio, "output", output);
    cJSON_AddItemToObject(audio, "input", input);
    cJSON_AddItemToObject(audio, "timbre", timbre);

    cJSON_AddStringToObject(server_info, "label", chat->mcp_label);
    cJSON_AddStringToObject(properties, "did", chat->device_id);
    cJSON_AddStringToObject(properties, "botId", chat->bot_id);

    if (jwt != NULL) {
        size_t bearer_len = strlen("Bearer ") + strlen(jwt) + 1;
        char *bearer = (char *)calloc(1, bearer_len);
        if (bearer == NULL) {
            free(jwt);
            cJSON_Delete(root);
            ESP_JOYINSIDE_UNLOCK(chat);
            return ESP_ERR_NO_MEM;
        }
        snprintf(bearer, bearer_len, "Bearer %s", jwt);
        cJSON_AddStringToObject(properties, "deviceToken", bearer);
        free(bearer);
    }

    cJSON_AddItemToObject(server_info, "properties", properties);
    cJSON_AddItemToObject(mcp, "serverInfo", server_info);
    cJSON_AddItemToObject(features, "mcp", mcp);

    cJSON_AddItemToObject(event_data, "audio", audio);
    cJSON_AddItemToObject(event_data, "features", features);
    cJSON_AddStringToObject(root, "eventType", "CLIENT_VOICE_CHAT_UPDATE");
    cJSON_AddItemToObject(root, "eventData", event_data);

    char *content_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(jwt);

    if (content_json == NULL) {
        ESP_JOYINSIDE_UNLOCK(chat);
        return ESP_ERR_NO_MEM;
    }

    char *json = joyinside_env_make_typed_json(chat->mid, "EVENT", content_json);
    free(content_json);
    if (json == NULL) {
        ESP_JOYINSIDE_UNLOCK(chat);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = send_json_text(chat, json, "joyinside.init");
    free(json);
    ESP_JOYINSIDE_UNLOCK(chat);
    return err;
}

esp_err_t esp_joyinside_proto_send_audio_finish(joyinside_chat_t *chat)
{
    ESP_RETURN_ON_FALSE(chat != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid chat");
    ESP_JOYINSIDE_LOCK(chat);
    char *json = joyinside_env_make_simple_event(chat->mid, "CLIENT_AUDIO_FINISH");
    if (json == NULL) {
        ESP_JOYINSIDE_UNLOCK(chat);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = send_json_text(chat, json, "joyinside.audio.finish");
    free(json);
    ESP_JOYINSIDE_UNLOCK(chat);
    return err;
}

esp_err_t esp_joyinside_proto_send_interrupt(joyinside_chat_t *chat)
{
    ESP_RETURN_ON_FALSE(chat != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid chat");
    ESP_JOYINSIDE_LOCK(chat);
    char *json = joyinside_env_make_simple_event(chat->mid, "CLIENT_INTERRUPT");
    if (json == NULL) {
        ESP_JOYINSIDE_UNLOCK(chat);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = send_json_text(chat, json, "joyinside.interrupt");
    free(json);
    ESP_JOYINSIDE_UNLOCK(chat);
    return err;
}
