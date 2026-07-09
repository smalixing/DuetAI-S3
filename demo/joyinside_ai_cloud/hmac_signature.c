#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include "mbedtls/md.h"

//#define TAG "hmac_signature"

#include "joyinside_log.h"


// 比较函数用于排序
static int compare_keys(const void *a, const void *b) {
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
}

int joyinside_cloud_signature(char *accessKeyID, char *accessKeySecret, char *accessNonce, 
                  char *accessTimestamp, char *botId, char hexdigest[33])
{
    if (!accessKeyID || !accessKeySecret || !accessNonce || !accessTimestamp) {
        return -1;
    }
    int calc_count = 5;
    joyinside_log_debug("accessKeyID: %s", accessKeyID);
    joyinside_log_debug("accessKeySecret: %s", accessKeySecret);
    joyinside_log_debug("accessNonce: %s", accessNonce);
    joyinside_log_debug("accessTimestamp: %s", accessTimestamp);
    if (botId) {
        joyinside_log_debug("botId: %s", botId);
    } else {
        joyinside_log_debug("Not the botid");
        calc_count = 4;
    }


    char *keys[] = {"accessVersion", "accessTimestamp", "accessNonce", "accessKeyId", "botId"};
    char *values[5];
    char *lower_keys[5];
    char *joint_str = NULL, *p;
    int max_len = 0, ret = 0, i, j;
    
    // accessVersion
    values[0] = "V2";
    // accessTimestamp (毫秒)
    values[1] = accessTimestamp;
    // accessNonce (UUID)
    values[2] = accessNonce;
    // accessKeyId
    values[3] = accessKeyID;
    //botID
    values[4] = botId;
    
    // 转换为小写并排序
    for (i = 0; i < calc_count; i++) {
        lower_keys[i] = strdup(keys[i]);
        for (p = lower_keys[i]; *p; p++) *p = tolower(*p);
        max_len += strlen(keys[i]) + strlen(values[i]) + 2; // 为&和=预留空间
    }

    qsort(lower_keys, calc_count, sizeof(char*), compare_keys);
    
    // 拼接参数字符串
    joint_str = (char *)malloc(max_len);
    if (!joint_str) {
        ret = -2;
        goto hmac_signature_finish;
    }
    memset(joint_str, 0x00, max_len);
    
    for (i = 0; i < calc_count; i++) {
        for (j = 0; j < calc_count; j++) {
            if (strcasecmp(keys[j], lower_keys[i]) == 0) {
                snprintf(joint_str+strlen(joint_str), max_len-strlen(joint_str), 
                        "%s=%s", lower_keys[i], values[j]);
                if (i < (calc_count-1)) { 
                    joint_str[strlen(joint_str)] = '&';
                }
                break;
            }
        }
    }
    joyinside_log_debug("sign str: %s", joint_str);
   
    // 使用mbedtls计算HMAC-MD5
    unsigned char digest[16];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info;

    mbedtls_md_init(&ctx);
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    
    if (mbedtls_md_setup(&ctx, md_info, 1) != 0) {
        ret = -3;
        goto hmac_signature_finish;
    }

    if (mbedtls_md_hmac_starts(&ctx, (const unsigned char *)accessKeySecret, 
                              strlen(accessKeySecret)) != 0) {
        ret = -4;
        goto hmac_signature_finish;
    }

    if (mbedtls_md_hmac_update(&ctx, (const unsigned char *)joint_str, 
                              strlen(joint_str)) != 0) {
        ret = -5;
        goto hmac_signature_finish;
    }

    if (mbedtls_md_hmac_finish(&ctx, digest) != 0) {
        ret = -6;
        goto hmac_signature_finish;
    }

    // 转换为十六进制字符串
    for (i = 0; i < 16; i++) {
        sprintf(hexdigest + 2*i, "%02x", digest[i]);
    }
    hexdigest[32] = '\0';
    
hmac_signature_finish:
    // 清理内存
    mbedtls_md_free(&ctx);
    
    for (i = 0; i < calc_count; i++) {
        if (lower_keys[i]) {
            free(lower_keys[i]);
            lower_keys[i] = NULL;
        }
        values[i] = NULL;
    }

    if (joint_str) {
        free(joint_str);
        joint_str = NULL;
    }
    
    return ret;
}