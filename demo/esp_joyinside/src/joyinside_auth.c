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

#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/base64.h"

#include "joyinside_auth.h"
#include "joyinside_jwt.h"
#include "joyinside_crypto.h"
#include "joyinside_envelope.h"

static const char *TAG = "JOYINSIDE_AUTH";

esp_err_t joyinside_crypto_hmac(psa_algorithm_t hash_alg,
                                const uint8_t *key,
                                size_t key_len,
                                const uint8_t *input,
                                size_t input_len,
                                uint8_t *mac,
                                size_t mac_size,
                                size_t *mac_len)
{
    ESP_RETURN_ON_FALSE(key != NULL && key_len > 0 && input != NULL && mac != NULL &&
                        mac_size > 0 && mac_len != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid HMAC arguments");

    psa_status_t status = psa_crypto_init();
    ESP_RETURN_ON_FALSE(status == PSA_SUCCESS, ESP_FAIL, TAG, "PSA init failed: %d", (int)status);
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;
    psa_algorithm_t algorithm = PSA_ALG_HMAC(hash_alg);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, algorithm);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, key_len * 8);
    status = psa_import_key(&attributes, key, key_len, &key_id);
    psa_reset_key_attributes(&attributes);
    ESP_RETURN_ON_FALSE(status == PSA_SUCCESS, ESP_FAIL, TAG, "HMAC key import failed: %d",
                        (int)status);

    status = psa_mac_compute(key_id, algorithm, input, input_len, mac, mac_size, mac_len);
    psa_status_t destroy_status = psa_destroy_key(key_id);
    ESP_RETURN_ON_FALSE(destroy_status == PSA_SUCCESS, ESP_FAIL, TAG, "HMAC key cleanup failed: %d",
                        (int)destroy_status);
    ESP_RETURN_ON_FALSE(status == PSA_SUCCESS, ESP_FAIL, TAG, "HMAC failed: %d", (int)status);
    return ESP_OK;
}

static int compare_keys(const void *a, const void *b)
{
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
}

static int now_timestamp_ms_string(char out[32])
{
    struct timeval tv = {0};
    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }
    unsigned long long ms = (unsigned long long)tv.tv_sec * 1000ULL +
                            (unsigned long long)tv.tv_usec / 1000ULL;
    snprintf(out, 32, "%llu", ms);
    return 0;
}

static void random_hex_string(char *out, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i] = hex[esp_random() & 0x0f];
    }
    out[len] = '\0';
}

static int joyinside_signature_hex(const char *access_key_id,
                                   const char *access_key_secret,
                                   const char *access_nonce,
                                   const char *access_timestamp,
                                   const char *bot_id,
                                   char hexdigest[33])
{
    char *keys[] = {"accessVersion", "accessTimestamp", "accessNonce", "accessKeyId", "botId"};
    char *values[5];
    char *lower_keys[5] = {0};
    char *joint_str = NULL;
    int calc_count = bot_id ? 5 : 4;
    int max_len = 0;
    int ret = -1;

    values[0] = "V2";
    values[1] = (char *)access_timestamp;
    values[2] = (char *)access_nonce;
    values[3] = (char *)access_key_id;
    values[4] = (char *)bot_id;

    for (int i = 0; i < calc_count; i++) {
        lower_keys[i] = strdup(keys[i]);
        if (lower_keys[i] == NULL) {
            goto out;
        }
        for (char *p = lower_keys[i]; *p; p++) {
            *p = (char)tolower((unsigned char)*p);
        }
        max_len += (int)strlen(keys[i]) + (int)strlen(values[i]) + 2;
    }

    qsort(lower_keys, calc_count, sizeof(char *), compare_keys);

    joint_str = (char *)calloc(1, (size_t)max_len + 1);
    if (joint_str == NULL) {
        goto out;
    }

    for (int i = 0; i < calc_count; i++) {
        for (int j = 0; j < calc_count; j++) {
            if (strcasecmp(keys[j], lower_keys[i]) == 0) {
                size_t used = strlen(joint_str);
                snprintf(joint_str + used, (size_t)max_len + 1 - used,
                         "%s=%s", lower_keys[i], values[j]);
                if (i + 1 < calc_count) {
                    strcat(joint_str, "&");
                }
                break;
            }
        }
    }

    unsigned char digest[16];
    size_t digest_len = 0;
    if (joyinside_crypto_hmac(PSA_ALG_MD5,
                              (const uint8_t *)access_key_secret,
                              strlen(access_key_secret),
                              (const uint8_t *)joint_str,
                              strlen(joint_str),
                              digest, sizeof(digest), &digest_len) != ESP_OK ||
        digest_len != sizeof(digest)) {
        goto out;
    }

    for (int i = 0; i < 16; i++) {
        sprintf(hexdigest + i * 2, "%02x", digest[i]);
    }
    hexdigest[32] = '\0';
    ret = 0;

out:
    for (int i = 0; i < calc_count; i++) {
        free(lower_keys[i]);
    }
    free(joint_str);
    return ret;
}

char *joyinside_auth_generate_uri(const char *ws_base_url,
                                  const char *access_key_id,
                                  const char *access_key_secret,
                                  const char *bot_id)
{
    if (ws_base_url == NULL || access_key_id == NULL ||
        access_key_secret == NULL || bot_id == NULL) {
        return NULL;
    }

    char nonce[17];
    char timestamp_str[32];
    char access_sign[33];
    char request_id[JOYINSIDE_EVENT_ID_BUF_LEN];
    char session_uuid[JOYINSIDE_EVENT_ID_BUF_LEN];

    random_hex_string(nonce, 16);
    if (now_timestamp_ms_string(timestamp_str) != 0) {
        return NULL;
    }
    if (joyinside_signature_hex(access_key_id, access_key_secret, nonce,
                                timestamp_str, bot_id, access_sign) != 0) {
        return NULL;
    }
    if (joyinside_env_gen_event_id(request_id, sizeof(request_id)) != ESP_OK ||
        joyinside_env_gen_event_id(session_uuid, sizeof(session_uuid)) != ESP_OK) {
        return NULL;
    }

    int session_need = snprintf(NULL, 0, "%s-%s", session_uuid, timestamp_str);
    if (session_need <= 0) {
        return NULL;
    }
    char *session_id = (char *)calloc(1, (size_t)session_need + 1);
    if (session_id == NULL) {
        return NULL;
    }
    snprintf(session_id, (size_t)session_need + 1, "%s-%s", session_uuid, timestamp_str);

    const char *fmt =
        "%s?interruptCleanAsrBuf=true&botId=%s&sessionId=%s&requestId=%s"
        "&accessKeyId=%s&accessTimestamp=%s&accessNonce=%s&accessVersion=V2"
        "&accessSign=%s";
    int need = snprintf(NULL, 0, fmt, ws_base_url, bot_id, session_id, request_id,
                        access_key_id, timestamp_str, nonce, access_sign);
    if (need <= 0) {
        free(session_id);
        return NULL;
    }

    char *uri = (char *)calloc(1, (size_t)need + 1);
    if (uri != NULL) {
        snprintf(uri, (size_t)need + 1, fmt, ws_base_url, bot_id, session_id,
                 request_id, access_key_id, timestamp_str, nonce, access_sign);
    }
    free(session_id);
    return uri;
}

static void base64_to_url_inplace(char *value, size_t *len)
{
    size_t count = *len;
    for (size_t i = 0; i < count; ++i) {
        if (value[i] == '+') {
            value[i] = '-';
        } else if (value[i] == '/') {
            value[i] = '_';
        }
    }
    while (count > 0 && value[count - 1] == '=') {
        value[--count] = '\0';
    }
    *len = count;
}

static char *base64url_encode_alloc(const unsigned char *input, size_t input_len)
{
    size_t capacity = ((input_len + 2) / 3) * 4 + 1;
    char *output = calloc(1, capacity);
    if (output == NULL) {
        return NULL;
    }
    size_t output_len = 0;
    if (mbedtls_base64_encode((unsigned char *)output, capacity, &output_len,
                              input, input_len) != 0) {
        free(output);
        return NULL;
    }
    base64_to_url_inplace(output, &output_len);
    return output;
}

static char *join_with_dot(const char *left, const char *right)
{
    size_t capacity = strlen(left) + strlen(right) + 2;
    char *output = calloc(1, capacity);
    if (output != NULL) {
        snprintf(output, capacity, "%s.%s", left, right);
    }
    return output;
}

char *joyinside_jwt_create(const char *did, const char *device_token, uint64_t timestamp_s)
{
    if (did == NULL || did[0] == '\0' || device_token == NULL || device_token[0] == '\0') {
        return NULL;
    }
    char payload[192];
    int payload_len = snprintf(payload, sizeof(payload),
                               "{\"did\":\"%s\",\"timestamp\":%llu}",
                               did, (unsigned long long)timestamp_s);
    if (payload_len <= 0 || payload_len >= (int)sizeof(payload)) {
        return NULL;
    }

    const char *header = "{\"typ\":\"JWT\",\"alg\":\"HS256\"}";
    char *header_b64 = base64url_encode_alloc((const unsigned char *)header, strlen(header));
    char *payload_b64 = base64url_encode_alloc((const unsigned char *)payload, (size_t)payload_len);
    char *header_and_payload = NULL;
    char *signature_b64 = NULL;
    char *jwt = NULL;
    if (header_b64 == NULL || payload_b64 == NULL) {
        goto out;
    }
    header_and_payload = join_with_dot(header_b64, payload_b64);
    if (header_and_payload == NULL) {
        goto out;
    }

    uint8_t digest[32];
    size_t digest_len = 0;
    if (joyinside_crypto_hmac(PSA_ALG_SHA_256,
                              (const uint8_t *)device_token, strlen(device_token),
                              (const uint8_t *)header_and_payload, strlen(header_and_payload),
                              digest, sizeof(digest), &digest_len) != ESP_OK ||
        digest_len != sizeof(digest)) {
        goto out;
    }
    signature_b64 = base64url_encode_alloc(digest, sizeof(digest));
    if (signature_b64 != NULL) {
        jwt = join_with_dot(header_and_payload, signature_b64);
    }

out:
    free(header_b64);
    free(payload_b64);
    free(header_and_payload);
    free(signature_b64);
    return jwt;
}
