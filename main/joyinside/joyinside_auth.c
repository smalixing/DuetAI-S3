/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/time.h>

#include "esp_random.h"

#include "mbedtls/md.h"

#include "hal_log.h"

#include "joyinside_auth.h"

#define AUTH_VERSION            "V2"
#define NONCE_LEN               (16)
#define TIMESTAMP_STR_LEN       (24)
#define UUID_STR_LEN            (36)
#define SIGN_HEX_LEN            (32)
#define AUTH_FIELD_COUNT        (5)

#define JWT_HEADER              "{\"typ\":\"JWT\",\"alg\":\"HS256\"}"
#define JWT_PAYLOAD_MAX         (128)
#define JWT_WORK_BUF_MAX        (256)
#define JWT_SIGN_LEN            (32)

static const char BASE64URL_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void _fill_random_string(char *out, int len)
{
    static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < len; i++) {
        out[i] = charset[esp_random() % (sizeof(charset) - 1)];
    }
    out[len] = '\0';
}

static uint64_t _epoch_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL;
}

static void _fill_timestamp_ms(char *out, int out_len)
{
    snprintf(out, out_len, "%llu", (unsigned long long)_epoch_ms());
}

void joyinside_auth_gen_uuid(char *out)
{
    /* RFC 4122-ish random UUID string built from esp_random() */
    uint8_t b[16];
    for (int i = 0; i < 16; i++) {
        b[i] = (uint8_t)(esp_random() & 0xff);
    }
    b[6] = (b[6] & 0x0f) | 0x40;
    b[8] = (b[8] & 0x3f) | 0x80;
    snprintf(out, UUID_STR_LEN + 1,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
             b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

static char *_base64url_encode(const unsigned char *input, size_t input_len)
{
    size_t out_size = ((input_len + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(out_size);
    if (out == NULL) {
        hal_log_err("base64url encode failed: OOM");
        return NULL;
    }

    size_t i = 0, j = 0;
    for (; i + 2 < input_len; i += 3) {
        unsigned char b1 = input[i], b2 = input[i + 1], b3 = input[i + 2];
        out[j++] = BASE64URL_TABLE[b1 >> 2];
        out[j++] = BASE64URL_TABLE[((b1 & 0x03) << 4) | (b2 >> 4)];
        out[j++] = BASE64URL_TABLE[((b2 & 0x0f) << 2) | (b3 >> 6)];
        out[j++] = BASE64URL_TABLE[b3 & 0x3f];
    }
    if (i < input_len) {
        unsigned char b1 = input[i];
        if (i + 1 == input_len) {
            out[j++] = BASE64URL_TABLE[b1 >> 2];
            out[j++] = BASE64URL_TABLE[(b1 & 0x03) << 4];
        } else {
            unsigned char b2 = input[i + 1];
            out[j++] = BASE64URL_TABLE[b1 >> 2];
            out[j++] = BASE64URL_TABLE[((b1 & 0x03) << 4) | (b2 >> 4)];
            out[j++] = BASE64URL_TABLE[(b2 & 0x0f) << 2];
        }
    }
    out[j] = '\0';
    return out;
}

static int _hmac(mbedtls_md_type_t type, const unsigned char *key, size_t key_len,
                 const unsigned char *data, size_t data_len, unsigned char *out)
{
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(type);
    if (info == NULL) {
        hal_log_err("HMAC failed: md info not found");
        return -1;
    }

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    int ret = -1;
    if (mbedtls_md_setup(&ctx, info, 1) != 0 ||
        mbedtls_md_hmac_starts(&ctx, key, key_len) != 0 ||
        mbedtls_md_hmac_update(&ctx, data, data_len) != 0 ||
        mbedtls_md_hmac_finish(&ctx, out) != 0) {
        hal_log_err("HMAC failed: mbedtls md error");
        goto exit;
    }
    ret = 0;

exit:
    mbedtls_md_free(&ctx);
    return ret;
}

static int _compare_keys(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/* HMAC-MD5 signature over lowercased-sorted "key=value&..." per JoyInside V2 auth */
static int _build_signature(const char *access_key_id, const char *access_key_secret,
                            const char *nonce, const char *timestamp, const char *bot_id,
                            char sign_hex[SIGN_HEX_LEN + 1])
{
    const char *keys[AUTH_FIELD_COUNT] =
        {"accessVersion", "accessTimestamp", "accessNonce", "accessKeyId", "botId"};
    const char *values[AUTH_FIELD_COUNT] =
        {AUTH_VERSION, timestamp, nonce, access_key_id, bot_id};

    char *lower_keys[AUTH_FIELD_COUNT] = {0};
    char *joint = NULL;
    int ret = -1;
    int max_len = 1;

    for (int i = 0; i < AUTH_FIELD_COUNT; i++) {
        lower_keys[i] = strdup(keys[i]);
        if (lower_keys[i] == NULL) {
            hal_log_err("Build signature failed: OOM");
            goto exit;
        }
        for (char *p = lower_keys[i]; *p; p++) {
            *p = (char)tolower((unsigned char)*p);
        }
        max_len += strlen(keys[i]) + strlen(values[i]) + 2;
    }

    qsort(lower_keys, AUTH_FIELD_COUNT, sizeof(char *), _compare_keys);

    joint = (char *)calloc(1, max_len);
    if (joint == NULL) {
        hal_log_err("Build signature failed: OOM");
        goto exit;
    }

    for (int i = 0; i < AUTH_FIELD_COUNT; i++) {
        for (int j = 0; j < AUTH_FIELD_COUNT; j++) {
            if (strcasecmp(keys[j], lower_keys[i]) == 0) {
                size_t cur = strlen(joint);
                snprintf(joint + cur, max_len - cur, "%s=%s", lower_keys[i], values[j]);
                if (i < AUTH_FIELD_COUNT - 1) {
                    joint[strlen(joint)] = '&';
                }
                break;
            }
        }
    }

    unsigned char digest[16];
    if (_hmac(MBEDTLS_MD_MD5, (const unsigned char *)access_key_secret,
              strlen(access_key_secret), (const unsigned char *)joint,
              strlen(joint), digest) != 0) {
        goto exit;
    }

    for (int i = 0; i < 16; i++) {
        sprintf(sign_hex + 2 * i, "%02x", digest[i]);
    }
    sign_hex[SIGN_HEX_LEN] = '\0';
    ret = 0;

exit:
    for (int i = 0; i < AUTH_FIELD_COUNT; i++) {
        free(lower_keys[i]);
    }
    free(joint);
    return ret;
}

char *joyinside_auth_build_uri(const char *base_uri, const char *access_key_id,
                               const char *access_key_secret, const char *bot_id)
{
    if (!base_uri || !access_key_id || !access_key_secret || !bot_id ||
        strlen(access_key_id) == 0 || strlen(access_key_secret) == 0 || strlen(bot_id) == 0) {
        hal_log_err("Build URI failed: missing credentials");
        return NULL;
    }

    char nonce[NONCE_LEN + 1] = {0};
    char timestamp[TIMESTAMP_STR_LEN] = {0};
    char request_id[UUID_STR_LEN + 1] = {0};
    char sign_hex[SIGN_HEX_LEN + 1] = {0};

    _fill_random_string(nonce, NONCE_LEN);
    _fill_timestamp_ms(timestamp, sizeof(timestamp));
    joyinside_auth_gen_uuid(request_id);

    if (_build_signature(access_key_id, access_key_secret, nonce, timestamp, bot_id, sign_hex) != 0) {
        hal_log_err("Build URI failed: signature error");
        return NULL;
    }

    char session_id[UUID_STR_LEN + TIMESTAMP_STR_LEN + 2] = {0};
    snprintf(session_id, sizeof(session_id), "%s-%s", request_id, timestamp);

    int uri_len = strlen(base_uri) +
                  strlen("?interruptCleanAsrBuf=true&botId=") + strlen(bot_id) +
                  strlen("&sessionId=") + strlen(session_id) +
                  strlen("&requestId=") + strlen(request_id) +
                  strlen("&accessKeyId=") + strlen(access_key_id) +
                  strlen("&accessTimestamp=") + strlen(timestamp) +
                  strlen("&accessNonce=") + strlen(nonce) +
                  strlen("&accessVersion=") + strlen(AUTH_VERSION) +
                  strlen("&accessSign=") + strlen(sign_hex) + 1;

    char *uri = (char *)malloc(uri_len);
    if (uri == NULL) {
        hal_log_err("Build URI failed: OOM");
        return NULL;
    }

    snprintf(uri, uri_len,
             "%s?interruptCleanAsrBuf=true&botId=%s&sessionId=%s&requestId=%s&accessKeyId=%s"
             "&accessTimestamp=%s&accessNonce=%s&accessVersion=%s&accessSign=%s",
             base_uri, bot_id, session_id, request_id, access_key_id,
             timestamp, nonce, AUTH_VERSION, sign_hex);

    return uri;
}

int joyinside_auth_build_jwt(const char *device_id, const char *device_token,
                             char *jwt_out, int jwt_out_len)
{
    if (!device_id || !device_token || !jwt_out) {
        hal_log_err("Build JWT failed: NULL argument");
        return -1;
    }

    char *header_b64 = NULL, *payload_b64 = NULL, *sign_b64 = NULL;
    int ret = -1;

    header_b64 = _base64url_encode((const unsigned char *)JWT_HEADER, strlen(JWT_HEADER));
    if (header_b64 == NULL) {
        goto exit;
    }

    char payload[JWT_PAYLOAD_MAX] = {0};
    uint64_t ts_sec = _epoch_ms() / 1000ULL;
    snprintf(payload, sizeof(payload), "{\"did\":\"%s\",\"timestamp\": %llu}",
             device_id, (unsigned long long)ts_sec);
    payload_b64 = _base64url_encode((const unsigned char *)payload, strlen(payload));
    if (payload_b64 == NULL) {
        goto exit;
    }

    char work[JWT_WORK_BUF_MAX] = {0};
    int n = snprintf(work, sizeof(work), "%s.%s", header_b64, payload_b64);
    if (n <= 0 || n >= (int)sizeof(work)) {
        hal_log_err("Build JWT failed: work buffer too small");
        goto exit;
    }

    unsigned char sign[JWT_SIGN_LEN];
    if (_hmac(MBEDTLS_MD_SHA256, (const unsigned char *)device_token, strlen(device_token),
              (const unsigned char *)work, strlen(work), sign) != 0) {
        goto exit;
    }
    sign_b64 = _base64url_encode(sign, sizeof(sign));
    if (sign_b64 == NULL) {
        goto exit;
    }

    n = snprintf(jwt_out, jwt_out_len, "%s.%s.%s", header_b64, payload_b64, sign_b64);
    if (n <= 0 || n >= jwt_out_len) {
        hal_log_err("Build JWT failed: output buffer too small");
        goto exit;
    }
    ret = 0;

exit:
    free(header_b64);
    free(payload_b64);
    free(sign_b64);
    return ret;
}
