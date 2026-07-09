
#include <stdio.h>
#include <stdlib.h>
#include <esp_err.h>

#include <string.h>

#include "mbedtls/md.h"

#include "joyinside_err.h"
#include "joyinside_log.h"
#include "joyinside_time.h"

#include "joyinside_hmac.h"

// Base64Url编码表（与标准base64不同，使用'-'和'_'替代'+'和'/'）
static const char base64url_table[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'
};
/**
 * Base64Url编码函数
 * @param input 输入数据（二进制数据）
 * @param input_len 输入数据长度
 * @param output_len 输出：编码后字符串长度（不包括终止符）
 * @return 编码后的字符串（需要调用者free），失败返回NULL
 */
static char* base64url_encode(const unsigned char* input, size_t input_len, size_t* output_len)
{
    if (input == NULL || output_len == NULL) {
        return NULL;
    }

    // 计算输出缓冲区大小（每3个字节编码为4个字符）
    size_t output_size = ((input_len + 2) / 3) * 4 + 1; // +1 for null terminator

    char* output = (char*)malloc(output_size);
    if (output == NULL) {
        return NULL;
    }

    size_t i = 0, j = 0;

    // 处理完整的3字节组
    for (; i + 2 < input_len; i += 3) {
        unsigned char byte1 = input[i];
        unsigned char byte2 = input[i + 1];
        unsigned char byte3 = input[i + 2];

        // 将3个字节（24位）分成4个6位组
        output[j++] = base64url_table[byte1 >> 2];
        output[j++] = base64url_table[((byte1 & 0x03) << 4) | (byte2 >> 4)];
        output[j++] = base64url_table[((byte2 & 0x0F) << 2) | (byte3 >> 6)];
        output[j++] = base64url_table[byte3 & 0x3F];
    }
    // 处理剩余字节
    if (i < input_len) {
        unsigned char byte1 = input[i];

        if (i + 1 == input_len) {
            // 剩余1个字节
            output[j++] = base64url_table[byte1 >> 2];
            output[j++] = base64url_table[(byte1 & 0x03) << 4];
            // 注意：base64url通常省略填充字符'='
        } else {
            // 剩余2个字节
            unsigned char byte2 = input[i + 1];

            output[j++] = base64url_table[byte1 >> 2];
            output[j++] = base64url_table[((byte1 & 0x03) << 4) | (byte2 >> 4)];
            output[j++] = base64url_table[(byte2 & 0x0F) << 2];
            // 注意：base64url通常省略填充字符'='
        }
    }

    output[j] = '\0'; // 添加字符串终止符
    *output_len = j;

    return output;
}

#define BOTID_REGISTER_PAYLOAD_SIZE     128
#define BOTID_REGISTER_BUFFER_SIZE      256
#define SIGNATURE_MAX_SIZE              32


#define BOTID_REGISTER_JWT_HEADER   "{\"typ\":\"JWT\",\"alg\":\"HS256\"}"
#define BOTID_REGISTER_JWT_PAYLOAD(buf, len, did, timestamp) \
    snprintf(buf, len, "{\"did\":\"%s\",\"timestamp\": %llu}", did, timestamp);

int xiot_jwt_calculation(const char *did, const char *device_token, char *jwt_data)
{
    if (did == NULL || device_token == NULL) {
        joyinside_log_err("did or device_token is null");
        return -1;
    }

    joyinside_log_debug(">>device_token: %s\n", device_token);

    uint64_t timestamp_s = joyinside_get_timestamp_sec();

    const char *jwt_header = BOTID_REGISTER_JWT_HEADER;

    size_t olen;
    char *header_base64 = base64url_encode((const unsigned char *)jwt_header, strlen(jwt_header), &olen);
    if (header_base64 == NULL) {
        joyinside_log_err("base64url_encode failed; data:%s", jwt_header);
    }

    char jwt_payload[BOTID_REGISTER_PAYLOAD_SIZE] = {0x00};
    int ret = BOTID_REGISTER_JWT_PAYLOAD(jwt_payload, BOTID_REGISTER_PAYLOAD_SIZE, did, timestamp_s);
    joyinside_log_debug("botid_register payload: %s", jwt_payload);
    if (ret >= BOTID_REGISTER_PAYLOAD_SIZE) {
        joyinside_log_err("buf(%d) is too small JWT PATLOAD(%d)", BOTID_REGISTER_PAYLOAD_SIZE, ret);
        return -1;
    }

    char *payload_base64 = base64url_encode((const unsigned char *)jwt_payload, strlen(jwt_payload), &olen);
    if (payload_base64 == NULL) {
        joyinside_log_err("base64url_encode failed; data:%s", jwt_payload);
    }

    char buff[BOTID_REGISTER_BUFFER_SIZE] = {0x00};
    ret = snprintf(buff, BOTID_REGISTER_BUFFER_SIZE, "%s.%s", header_base64, payload_base64);

    uint8_t output[SIGNATURE_MAX_SIZE];
    joyinside_err_t err = hmac_sha256_sign((const unsigned char *)device_token, strlen(device_token), (uint8_t *)buff, strlen(buff), output);
    if (err != JOYINSIDE_ERR_OK) {
        joyinside_log_err("hmac_sha256_sign failed");
        goto exit;
    }

    char *signature_base64 = base64url_encode((const unsigned char *)output, sizeof(output), &olen);
    if (signature_base64 == NULL) {
        joyinside_log_err("base64url_encode failed; signature_base64");
    }
    joyinside_log_debug("signature_base64:%s", signature_base64);


    int len = strlen(buff);
    ret = snprintf(buff + len, BOTID_REGISTER_BUFFER_SIZE - len, ".%s", signature_base64);
    joyinside_log_debug("botid_register jwt: %s", buff);
    strcpy(jwt_data, buff);

    if (signature_base64) {
        free(signature_base64);
    }

exit:
    if (header_base64) {
        free(header_base64);
    }
    if (payload_base64) {
        free(payload_base64);
    }
    return 0;
}
