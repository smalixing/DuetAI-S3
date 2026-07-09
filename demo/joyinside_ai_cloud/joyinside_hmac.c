
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "mbedtls/md.h"
#include "mbedtls/md5.h"

#include "joyinside_err.h"
#include "joyinside_log.h"


joyinside_err_t hmac_sha256_sign(const unsigned char *key, size_t key_len, const unsigned char *data, size_t data_len, uint8_t *output)
{
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        joyinside_log_err("Failed to get MD info for SHA256");
        return JOYINSIDE_ERR_INVALID_ARG;
    }

    mbedtls_md_setup(&ctx, md_info, 1);
    int ret = mbedtls_md_hmac_starts(&ctx, key, key_len);
    if (ret != 0) {
        joyinside_log_err("HMAC start failed with error code: %d", ret);
        mbedtls_md_free(&ctx);
        return JOYINSIDE_ERR_FAIL;
    }

    ret = mbedtls_md_hmac_update(&ctx, data, data_len);
    if (ret != 0) {
        joyinside_log_err("HMAC update failed with error code: %d", ret);
        mbedtls_md_free(&ctx);
        return JOYINSIDE_ERR_FAIL;
    }

    ret = mbedtls_md_hmac_finish(&ctx, output);
    if (ret != 0) {
        joyinside_log_err("HMAC finish failed with error code: %d", ret);
        mbedtls_md_free(&ctx);
        return JOYINSIDE_ERR_FAIL;
    }

    mbedtls_md_free(&ctx);
    return JOYINSIDE_OK;
}

joyinside_err_t hmac_md5_sign(const unsigned char *key, size_t key_len, const unsigned char *data, size_t data_len, uint8_t *output)
{
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    if (md_info == NULL) {
        joyinside_log_err("Failed to get MD info for SHA256");
        return JOYINSIDE_ERR_INVALID_ARG;
    }

    mbedtls_md_setup(&ctx, md_info, 1);
    int ret = mbedtls_md_hmac_starts(&ctx, key, key_len);
    if (ret != 0) {
        joyinside_log_err("HMAC start failed with error code: %d", ret);
        mbedtls_md_free(&ctx);
        return JOYINSIDE_ERR_FAIL;
    }

    ret = mbedtls_md_hmac_update(&ctx, data, data_len);
    if (ret != 0) {
        joyinside_log_err("HMAC update failed with error code: %d", ret);
        mbedtls_md_free(&ctx);
        return JOYINSIDE_ERR_FAIL;
    }

    ret = mbedtls_md_hmac_finish(&ctx, output);
    if (ret != 0) {
        joyinside_log_err("HMAC finish failed with error code: %d", ret);
        mbedtls_md_free(&ctx);
        return JOYINSIDE_ERR_FAIL;
    }

    mbedtls_md_free(&ctx);
    return JOYINSIDE_OK;
}

char* bytes_to_hex(const uint8_t* bytes, int length) 
{
    if(!bytes || length <= 0) return NULL;
    
    const char* hex_chars = "0123456789abcdef";
    char* hex_string = malloc(length * 2 + 1);
    
    if(!hex_string) return NULL;
    
    for(int i = 0; i < length; i++) {
        hex_string[i * 2] = hex_chars[(bytes[i] >> 4) & 0x0F];
        hex_string[i * 2 + 1] = hex_chars[bytes[i] & 0x0F];
    }
    hex_string[length * 2] = '\0';
    
    return hex_string;
}

int md5_sum(char *data, int len, char hash[33])
{
    unsigned char _hash_hex[16] = {0x00};
    mbedtls_md5_context _md5_ctx;
    mbedtls_md5_init(&_md5_ctx);
    mbedtls_md5_starts(&_md5_ctx);
    mbedtls_md5_update(&_md5_ctx, (unsigned char *)data, len);
    mbedtls_md5_finish(&_md5_ctx, _hash_hex);
    mbedtls_md5_free(&_md5_ctx);
    
    for (int i = 0; i < 16; i++) {
        sprintf(hash + (i * 2), "%02x", _hash_hex[i]);
    }
    hash[32] = '\0';
    return 0;
}