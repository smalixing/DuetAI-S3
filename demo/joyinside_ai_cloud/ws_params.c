#include <esp_random.h>
#include <esp_system.h>
#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>

#include "ws_params.h"
#include "joyinside_hmac.h"
#include "sys/time.h"

#include "joyinside_log.h"
#include "joyinside_err.h"
#include "joyinside_time.h"

#include "hmac_signature.h"

char* ws_generate_auth_params(const char *accessKeyID,      // 应用标识
                            const char *accessKeySecret,   // 应用密钥
                            const char *botid,            // 机器人标识
                            const char *version,          // 认证版本, 
                            char access_sign[33])
{
    joyinside_log_info("Starting authentication parameters generation");
    joyinside_log_debug("Auth config - Version: %s, KeyID: %s, KeySecret:%s, BotID: %s",
              version ? version : "NULL",
              accessKeyID ? accessKeyID : "NULL",
              accessKeySecret ? accessKeySecret: "NULL",              
              botid ? botid : "NULL");
    if ( !version || !accessKeyID || !accessKeySecret || !botid) {
        joyinside_log_err("Invalid authentication configuration");
        return NULL;
    }

    if (!version || strcasecmp(version, "V2") != 0) {
        joyinside_log_err("Unsupported auth version: %s (only V2 is supported)",
                  version ? version : "NULL");
        return NULL;
    }

    //Version V2 params 
    char *params = NULL;
    #define NONCE_LEN 16
    char nonce[NONCE_LEN+1] = {0x00};
    if (joyinside_random_string(nonce, sizeof(nonce)-1) != 0) {
        joyinside_log_err("Failed to generate nonce");
        goto ws_generate_auth_params_finish;
    }
    joyinside_log_debug("Generated nonce successfully: %s", nonce);

#define TIMESTAMP_STRING_LEN 32
    char timestamp_str[TIMESTAMP_STRING_LEN] = {0x00};
    joyinside_timestamp_ms_str(timestamp_str);

    char signature[33] = {0}; // 32 chars for hex string + null terminator
    if (joyinside_cloud_signature((char *)accessKeyID, (char *)accessKeySecret, nonce, timestamp_str, (char *)botid, signature) != 0) {
        joyinside_log_err("HMAC signature generation failed - KeyID: %s, Nonce: %s, Timestamp: %s",
                  accessKeyID, nonce, timestamp_str);
        goto ws_generate_auth_params_finish;
    }
    joyinside_log_debug("Generated HMAC signature successfully: %s", signature);
    strncpy(access_sign, signature, strlen(signature));

    char request_id[UUID_STRING_LEN+1] = {0x00};
    if (uuid_generate(request_id) != 0) {
        joyinside_log_err("Failed to generate request ID");
        goto ws_generate_auth_params_finish;
    }
    joyinside_log_debug("Generated request ID: %s", request_id);
    char session_id[UUID_STRING_LEN+TIMESTAMP_STRING_LEN+2] = {0x00};
    int session_id_len = strlen(request_id) + strlen(timestamp_str) + 2;
    snprintf(session_id, session_id_len, "%s-%s", request_id, timestamp_str);
    session_id[session_id_len-1] = '\0';

    // 计算URL参数字符串的总长度
    size_t params_len = strlen("?interruptCleanAsrBuf=true&botId=") + strlen(botid) +
                       strlen("&sessionId=") + strlen(session_id) +
                       strlen("&requestId=") + strlen(request_id) +
                       strlen("&accessKeyId=") + strlen(accessKeyID) +
                       strlen("&accessTimestamp=") + strlen(timestamp_str) +
                       strlen("&accessNonce=") + strlen(nonce) +
                       strlen("&accessVersion=") + strlen(version) +
                       strlen("&accessSign=") + strlen((char *)signature) + 1 + 
                       strlen("&interruptCleanAsrBuf=true") + 1;

    params = (char*)malloc(params_len);
    if (!params) {
        joyinside_log_err("Failed to allocate memory for auth parameters");
        goto ws_generate_auth_params_finish;
    }
    memset(params, 0x00, params_len);

    // 构造URL参数字符串
    snprintf(params, params_len,
             "?interruptCleanAsrBuf=true&botId=%s&sessionId=%s&requestId=%s&accessKeyId=%s"
             "&accessTimestamp=%s&accessNonce=%s&accessVersion=%s&accessSign=%s&interruptCleanAsrBuf=true",
             botid, session_id, request_id, accessKeyID,
             timestamp_str, nonce, version, signature);

ws_generate_auth_params_finish:

    if (params) {
        joyinside_log_info("Authentication parameters generated successfully");
    } else {
        joyinside_log_err("Failed to generate authentication parameters");
    }
    return params;
}
