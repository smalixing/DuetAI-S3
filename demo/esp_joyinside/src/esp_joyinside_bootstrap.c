/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/base64.h"
#include "psa/crypto.h"

#include "esp_joyinside_bootstrap.h"
#include "joyinside_crypto.h"

static const char *TAG = "ESP_JOYINSIDE_BOOTSTRAP";

typedef struct {
    char  *data;
    size_t len;
} response_buffer_t;

static esp_err_t append_response(response_buffer_t *response, const char *data, int len)
{
    char *next = realloc(response->data, response->len + (size_t)len + 1);
    ESP_RETURN_ON_FALSE(next != NULL, ESP_ERR_NO_MEM, TAG, "no memory for HTTP response");
    response->data = next;
    memcpy(response->data + response->len, data, (size_t)len);
    response->len += (size_t)len;
    response->data[response->len] = '\0';
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data != NULL &&
        event->data_len > 0 && event->user_data != NULL) {
        return append_response(event->user_data, event->data, event->data_len);
    }
    return ESP_OK;
}

static esp_err_t post_json(const char *url, const char *bearer_token, const char *body,
                           int timeout_ms, char **out_response)
{
    response_buffer_t response = {0};
    esp_http_client_config_t client_config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &response,
        .timeout_ms = timeout_ms > 0 ? timeout_ms : 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    ESP_RETURN_ON_FALSE(client != NULL, ESP_FAIL, TAG, "HTTP client init failed");

    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (bearer_token != NULL) {
        size_t len = strlen("Bearer ") + strlen(bearer_token) + 1;
        char *authorization = calloc(1, len);
        if (authorization == NULL) {
            esp_http_client_cleanup(client);
            return ESP_ERR_NO_MEM;
        }
        snprintf(authorization, len, "Bearer %s", bearer_token);
        esp_http_client_set_header(client, "Authorization", authorization);
        free(authorization);
    }
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status < 200 || status >= 300) {
            ESP_LOGW(TAG, "HTTP request failed with status %d", status);
            err = ESP_FAIL;
        }
    }
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        free(response.data);
        return err;
    }
    *out_response = response.data;
    return ESP_OK;
}

static void random_hex(char *out, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i] = hex[esp_random() & 0xf];
    }
    out[len] = '\0';
}

static esp_err_t timestamp_ms(char out[32])
{
    struct timeval now = {0};
    ESP_RETURN_ON_FALSE(gettimeofday(&now, NULL) == 0, ESP_FAIL, TAG, "gettimeofday failed");
    ESP_RETURN_ON_FALSE(now.tv_sec >= 1577836800, ESP_ERR_INVALID_STATE, TAG,
                        "system time is not synchronized");
    snprintf(out, 32, "%llu", (unsigned long long)now.tv_sec * 1000ULL +
             (unsigned long long)now.tv_usec / 1000ULL);
    return ESP_OK;
}

static esp_err_t sign_v2(const char *access_key_id, const char *access_key_secret,
                         const char *nonce, const char *timestamp, const char *bot_id,
                         char signature[33])
{
    const char *keys[] = {"accessVersion", "accessTimestamp", "accessNonce", "accessKeyId", "botId"};
    const char *values[] = {"V2", timestamp, nonce, access_key_id, bot_id};
    const size_t count = bot_id ? 5 : 4;
    const size_t order[] = {3, 2, 1, 0, 4};
    size_t capacity = 1;

    for (size_t i = 0; i < count; ++i) {
        capacity += strlen(keys[i]) + strlen(values[i]) + 2;
    }
    char *input = calloc(1, capacity);
    ESP_RETURN_ON_FALSE(input != NULL, ESP_ERR_NO_MEM, TAG, "no memory for signature");
    for (size_t i = 0; i < count; ++i) {
        const size_t index = order[i];
        snprintf(input + strlen(input), capacity - strlen(input), "%s=%s%s",
                 index == 3 ? "accesskeyid" :
                 index == 2 ? "accessnonce" :
                 index == 1 ? "accesstimestamp" :
                 index == 0 ? "accessversion" : "botid",
                 values[index], i + 1 < count ? "&" : "");
    }

    uint8_t digest[16];
    size_t digest_len = 0;
    esp_err_t err = joyinside_crypto_hmac(PSA_ALG_MD5,
                                          (const uint8_t *)access_key_secret,
                                          strlen(access_key_secret),
                                          (const uint8_t *)input, strlen(input),
                                          digest, sizeof(digest), &digest_len);
    free(input);
    ESP_RETURN_ON_FALSE(err == ESP_OK && digest_len == sizeof(digest), ESP_FAIL, TAG,
                        "V2 signature failed");
    for (size_t i = 0; i < sizeof(digest); ++i) {
        snprintf(signature + i * 2, 3, "%02x", digest[i]);
    }
    signature[32] = '\0';
    return ESP_OK;
}

static esp_err_t get_access_token(const esp_joyinside_bootstrap_config_t *config, char **out_token)
{
    char timestamp[32];
    char nonce[33];
    char signature[33];
    ESP_RETURN_ON_ERROR(timestamp_ms(timestamp), TAG, "get timestamp");
    random_hex(nonce, sizeof(nonce) - 1);
    ESP_RETURN_ON_ERROR(sign_v2(config->access_key_id, config->access_key_secret, nonce,
                                timestamp, NULL, signature), TAG, "sign token request");

    int size = snprintf(NULL, 0,
                        "{\"accessVersion\":\"V2\",\"accessTimestamp\":\"%s\","
                        "\"accessNonce\":\"%s\",\"accessKeyId\":\"%s\",\"accessSign\":\"%s\","
                        "\"vendorId\":\"%s\"}",
                        timestamp, nonce, config->access_key_id, signature, config->vendor_id);
    ESP_RETURN_ON_FALSE(size > 0, ESP_FAIL, TAG, "token request formatting failed");
    char *body = calloc(1, (size_t)size + 1);
    ESP_RETURN_ON_FALSE(body != NULL, ESP_ERR_NO_MEM, TAG, "no memory for token request");
    snprintf(body, (size_t)size + 1,
             "{\"accessVersion\":\"V2\",\"accessTimestamp\":\"%s\",\"accessNonce\":\"%s\","
             "\"accessKeyId\":\"%s\",\"accessSign\":\"%s\",\"vendorId\":\"%s\"}",
             timestamp, nonce, config->access_key_id, signature, config->vendor_id);

    char *response = NULL;
    esp_err_t err = post_json(config->get_token_url ? config->get_token_url :
                              ESP_JOYINSIDE_DEFAULT_GET_TOKEN_URL, NULL, body,
                              config->timeout_ms, &response);
    free(body);
    ESP_RETURN_ON_ERROR(err, TAG, "get token request failed");
    cJSON *root = cJSON_Parse(response);
    free(response);
    ESP_RETURN_ON_FALSE(root != NULL, ESP_FAIL, TAG, "token response is not JSON");
    cJSON *token = cJSON_GetObjectItem(root, "accessToken");
    *out_token = cJSON_IsString(token) && token->valuestring ? strdup(token->valuestring) : NULL;
    cJSON_Delete(root);
    return *out_token ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_joyinside_bootstrap_generate_device_id(esp_mac_type_t mac_type, char **out_device_id)
{
    ESP_RETURN_ON_FALSE(out_device_id != NULL, ESP_ERR_INVALID_ARG, TAG, "device ID output is null");
    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, mac_type), TAG, "read device MAC");
    size_t capacity = ((sizeof(mac) + 2) / 3) * 4 + 1;
    char *device_id = calloc(1, capacity);
    ESP_RETURN_ON_FALSE(device_id != NULL, ESP_ERR_NO_MEM, TAG, "no memory for device ID");
    size_t output_len = 0;
    if (mbedtls_base64_encode((uint8_t *)device_id, capacity, &output_len, mac, sizeof(mac)) != 0) {
        free(device_id);
        return ESP_FAIL;
    }
    device_id[output_len] = '\0';
    *out_device_id = device_id;
    return ESP_OK;
}

esp_err_t esp_joyinside_bootstrap_register(const esp_joyinside_bootstrap_config_t *config,
                                           char **out_bot_id)
{
    ESP_RETURN_ON_FALSE(config != NULL && out_bot_id != NULL && config->access_key_id != NULL &&
                        config->access_key_secret != NULL && config->vendor_id != NULL &&
                        config->app_id != NULL && config->device_model != NULL &&
                        config->device_id != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "required bootstrap fields missing");
    if (config->struct_version != 0 &&
        config->struct_version != ESP_JOYINSIDE_BOOTSTRAP_CONFIG_VERSION) {
        ESP_LOGE(TAG, "Register failed: unsupported config version %lu",
                 (unsigned long)config->struct_version);
        return ESP_ERR_INVALID_ARG;
    }
    *out_bot_id = NULL;

    char *access_token = NULL;
    ESP_RETURN_ON_ERROR(get_access_token(config, &access_token), TAG, "get access token");
    uint8_t digest[16];
    size_t digest_len = 0;
    if (psa_hash_compute(PSA_ALG_MD5, (const uint8_t *)config->device_id,
                         strlen(config->device_id), digest, sizeof(digest), &digest_len) != PSA_SUCCESS ||
        digest_len != sizeof(digest)) {
        free(access_token);
        return ESP_FAIL;
    }
    char device_id_hash[33];
    for (size_t i = 0; i < sizeof(digest); ++i) {
        snprintf(device_id_hash + i * 2, 3, "%02x", digest[i]);
    }
    device_id_hash[32] = '\0';

    int size = snprintf(NULL, 0,
                        "{\"appId\":\"%s\",\"deviceId\":\"%s#%s\","
                        "\"type\":\"PHYSICAL_ROBOT\",\"vendorId\":\"%s\",\"name\":\"%s\"}",
                        config->app_id, config->device_id, device_id_hash,
                        config->vendor_id, config->device_model);
    if (size <= 0) {
        free(access_token);
        return ESP_FAIL;
    }
    char *body = calloc(1, (size_t)size + 1);
    if (body == NULL) {
        free(access_token);
        return ESP_ERR_NO_MEM;
    }
    snprintf(body, (size_t)size + 1,
             "{\"appId\":\"%s\",\"deviceId\":\"%s#%s\",\"type\":\"PHYSICAL_ROBOT\","
             "\"vendorId\":\"%s\",\"name\":\"%s\"}",
             config->app_id, config->device_id, device_id_hash,
             config->vendor_id, config->device_model);

    char *response = NULL;
    esp_err_t err = post_json(config->device_register_url ? config->device_register_url :
                              ESP_JOYINSIDE_DEFAULT_DEVICE_REGISTER_URL,
                              access_token, body, config->timeout_ms, &response);
    free(body);
    free(access_token);
    ESP_RETURN_ON_ERROR(err, TAG, "device registration failed");
    cJSON *root = cJSON_Parse(response);
    free(response);
    ESP_RETURN_ON_FALSE(root != NULL, ESP_FAIL, TAG, "register response is not JSON");
    cJSON *state = cJSON_GetObjectItem(root, "state");
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (cJSON_IsString(state) && strcmp(state->valuestring, "SUCCESS") == 0 &&
        cJSON_IsString(data) && data->valuestring != NULL) {
        *out_bot_id = strdup(data->valuestring);
    }
    cJSON_Delete(root);
    return *out_bot_id ? ESP_OK : ESP_FAIL;
}
