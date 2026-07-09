#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal_http.h"

#include "hmac_signature.h"
#include "joyinside_time.h"
#include "params.h"
#include "joyinside_log.h"
#include "joyinside_kv.h"
#include "dev_info.h"
#include "joyinside_err.h"
#include "joyinside_hmac.h"

#include "cJSON.h"

#include "version.h"

static int request_botid_cb(void *param, unsigned char *buf, int len, int offset, int content_length)
{
    int ret = 0;
    printf("get botid recv: %s\n", buf);
    //{"state":"SUCCESS","code":"0000","data":"b44feb8d4a0445daa08635b6cc06d55f"}
    cJSON *root_obj = cJSON_Parse((char *)buf);
    if (!root_obj) {
        ret = -1;
        goto request_botid_cb_end;
    }
    cJSON *state_obj = cJSON_GetObjectItem(root_obj, "state");
    if (!state_obj || !cJSON_IsString(state_obj) || state_obj->valuestring == NULL || 
        (strncmp(state_obj->valuestring, "SUCCESS", strlen(state_obj->valuestring)) != 0)) {
        ret = -1;
        goto request_botid_cb_end;
    }
    cJSON *botid_obj = cJSON_GetObjectItem(root_obj, "data");
    if (!botid_obj || !cJSON_IsString(botid_obj) || botid_obj->valuestring == NULL) {
        ret = -1;
        goto request_botid_cb_end;
    }
    if (set_info_by_key(JOYINSIDE_KV_JOYINSIDE_BOTID, botid_obj->valuestring, strlen(botid_obj->valuestring)) != JOYINSIDE_OK) {
        ret = -1;
        joyinside_log_warn("Save the botid error, the botid :%s", botid_obj->valuestring);
        goto request_botid_cb_end;
    }

    joyinside_log_info("Save the %s: %s success.", JOYINSIDE_KV_JOYINSIDE_BOTID, botid_obj->valuestring);    

    ret = 0;

request_botid_cb_end:
    if (root_obj) {
        cJSON_Delete(root_obj);
    }
    return (ret == 0)?len:0;
}

static int get_token_cb(void *param, unsigned char *buf, int len, int offset, int content_length)
{
    printf("get token recv: %s\n", buf);
    int ret = 0;
    char *header = NULL,  *request_json = NULL;
    cJSON *root_obj = cJSON_Parse((char *)buf);
    if (!root_obj) {
        joyinside_log_err("Response joyinside cloud token ins't Json.");
        ret = -1;
        goto get_token_cb_end;
    }
    cJSON *token_obj = cJSON_GetObjectItem(root_obj, "accessToken");
    if (!token_obj || !cJSON_IsString(token_obj) || !token_obj->valuestring) {
        joyinside_log_err("Response joyinside cloud token not find the accessToken.");
        ret = -1;
        goto get_token_cb_end;
    }

    char appid[32] = {0x00}, vendorid[32] = {0x00};
    if (get_info_by_key(JOYINSIDE_KV_JOYINSIDE_APPID, appid, sizeof(appid)) != JOYINSIDE_OK) {
        joyinside_log_err("Response joyinside cloud token not find the appid.");
        ret = -1;
        goto get_token_cb_end;
    }
    
    if (get_info_by_key(JOYINSIDE_KV_JOYINSIDE_VENDORID, vendorid, sizeof(vendorid)) != JOYINSIDE_OK) {
        joyinside_log_err("Response joyinside cloud token not find the appid.");
        ret = -1;
        goto get_token_cb_end;
    }
    
    header = malloc(1024);
    if (!header) {
        joyinside_log_err("OOM at alloc request botid header.");
        ret = -1;
        goto get_token_cb_end;
    }
    memset(header, 0x00, 1024);

    request_json = malloc(1024);
    if (!request_json) {
        joyinside_log_err("OOM at alloc request botid header.");
        ret = -1;
        goto get_token_cb_end;
    }
    memset(request_json, 0x00, 1024);

    snprintf(header, 1024, 
            "Authorization: Bearer %s\r\n"
            "Content-Type: application/json; charset=utf-8", token_obj->valuestring);

    char _did_md5[33] = {0x00};
    char *_did = global_get_did();
    if (_did) {
        md5_sum(_did, strlen(_did), _did_md5);
    }
    snprintf(request_json, 1024,
            "{\"appId\": \"%s\",\"deviceId\":\"%s#%s\","
             "\"type\":\"PHYSICAL_ROBOT\",\"vendorId\":\"%s\","
             "\"name\":\"JD_AIoT\"}", appid, _did, _did_md5, vendorid);

    printf("reqeuest botid json: %s\n", request_json);

#ifndef TEST_JOYINSIDE_CLOUD
#define DEVICE_REGISTER "https://joyinside.jd.com/device/register"
#else
#define DEVICE_REGISTER "https://pre-joyinside-out.3.cn/device/register"
#endif    

    ret = hal_http_post(DEVICE_REGISTER, 0, header, request_json, strlen(request_json), request_botid_cb, NULL);
    if (ret == JOYINSIDE_OK) {
        ret = len;
    } else {
        ret = -1; //error
    }

get_token_cb_end:
    if (header) {
        free(header);
    }
    if (request_json) {
        free(request_json);
    }
    cJSON_Delete(root_obj);
  
    return ret;
}

int request_botid(char *acccesskey_id, char *accesskey_secert, char *did, char *vendorid)
{
    joyinside_log_debug(">>>get token did: %s", did?did:"NULL");

    char *buf = malloc(1024);
    char timestamp_str[32] = {0x00};
    char nonce[33] = {0x00};
    char sign[33] = {0x00};

    joyinside_timestamp_ms_str(timestamp_str);
    joyinside_random_string(nonce, sizeof(nonce)-1);

    int ret = joyinside_cloud_signature(acccesskey_id, accesskey_secert, nonce, timestamp_str, NULL, sign);
    if ( ret != 0) {
        joyinside_log_debug("joyinside_cloud_signature error %d.", ret);
    }

    snprintf(buf, 1024,
        "{\"accessVersion\":\"V2\",\"accessTimestamp\":\"%s\","
        "\"accessNonce\":\"%s\",\"accessKeyId\":\"%s\",\"accessSign\":\"%s\","
        "\"vendorId\":\"%s\"}",
        timestamp_str, nonce, acccesskey_id, sign, vendorid);

#ifndef TEST_JOYINSIDE_CLOUD
#define GET_TOKEN_URL "https://joyinside.jd.com/auth/getToken"
#else
#define GET_TOKEN_URL "https://pre-joyinside-out.3.cn/auth/getToken"
#endif
#define GET_TOKEN_HEADER "Content-Type: application/json"
    
    ret = hal_http_post(GET_TOKEN_URL, 0, GET_TOKEN_HEADER, buf, strlen(buf), get_token_cb, (void *)did);
    free(buf);
    return ret;
}