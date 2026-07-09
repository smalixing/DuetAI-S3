#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mbedtls/base64.h>

#include "esp_heap_caps.h"
#include "audio_message.h"
#include "joyinside_log.h"
#include "audio_message.h"
#include "dev_info.h"
#include "joyinside_err.h"

#include "joyinside_hmac.h"

#include "jwt.h"

#define AUDIO_FRAME_MAX (8*1024)
#define AUDIO_MAX (AUDIO_FRAME_MAX * 3 / 4)

#define AUDIO_FRAME_JSON_HEAD 512
#define JWT_BUFFER_SIZE      256

extern int joyinside_ws_write_text(char *text, int len);

audio_message_t *audio_message_create(const char *uid) 
{
    //int uid_len = strlen(uid) + 1;
    int uid_len = 0;

    int len = uid_len + sizeof(audio_message_t) + AUDIO_FRAME_MAX + AUDIO_FRAME_JSON_HEAD;
    audio_message_t *msg = (audio_message_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if ( !msg ) {
        joyinside_log_err("OOM at alloc audio message handler(%d).", len);
        return NULL;
    }
    memset(msg, 0x00, len);
    // 生成消息ID
    uuid_generate( msg->mid );
    msg->send_buffer = (char *)msg + sizeof(audio_message_t) + uid_len;
    msg->send_buffer_len = AUDIO_FRAME_MAX + AUDIO_FRAME_JSON_HEAD;
    //joyinside_log_debug("sendbuf: %p", msg->send_buffer);
    return msg;
}

int audio_message_send(void *ctx, audio_message_t *msg, 
                      const uint8_t *audio_data, size_t data_len) {
    //joyinside_log_debug("start send audio(%d).", data_len);
    if (!msg || !audio_data || data_len == 0) {
        joyinside_log_err("Invalid parameters, audio_message_send");
        return -1;
    }
    //joyinside_log_debug("sendbuf: %p", msg->send_buffer);
    memset(msg->send_buffer, 0x00, msg->send_buffer_len);
    data_len = data_len > AUDIO_MAX? AUDIO_MAX: data_len;
    //joyinside_log_debug("audio send data len: %d", data_len);
    //printf("uid(%p)(%d): %s\n", msg->uid, strlen(msg->uid), msg->uid);
    size_t json_len = snprintf(msg->send_buffer, msg->send_buffer_len, 
        "{\"mid\":\"%s\",\"contentType\":\"AUDIO\","
        "\"content\":{\"index\":%d,\"audioBase64\":\"", msg->mid, msg->index);
    //printf("json(%d): %s\n", json_len, msg->send_buffer);
    // Base64编码音频数据
    size_t olen = 0;
    if (mbedtls_base64_encode((unsigned char*)msg->send_buffer + json_len, 
                                msg->send_buffer_len - json_len - 1, 
                                &olen, audio_data, data_len) != 0) {
        joyinside_log_err("Failed to encode audio data to base64");
        return -1;
    }
    json_len = strlen(msg->send_buffer);
    json_len += snprintf(msg->send_buffer + json_len, msg->send_buffer_len - json_len - 1, "%s", "\"}}");
   
    msg->index ++;
    if (msg->index % 100 == 0) {
        joyinside_log_debug("send msg index: %d, audio len: %d", msg->index, json_len);
    }

    //printf("audio send(%d): %s\n", json_len, msg->send_buffer);
    if (joyinside_ws_write_text(msg->send_buffer, json_len) != 0 ) {
        joyinside_log_err("joyinside_ws_write_text err.\n");
        return -1;
    }

    /*
    joyinside_log_debug("Audio message sent successfully: mid=%s, index=%d, len %d", 
              msg->mid, msg->index - 1, json_len);
    */
    //joyinside_log_debug("send audio(%d) end.", data_len);
    return 0;
}
extern char *get_dev_token();
extern char *get_botid();

int audio_init_event_send(void *ctx, audio_message_t *msg)
{
    int ret = 0;
    char *did = global_get_did();
    char *dev_token = get_dev_token();
    char *botid = get_botid();
 
    if (!did || !dev_token || !botid) {
        joyinside_log_info("Not find the botid.");
        return -1;
    }
    #if 0
    char _md5[33] = {0x00};
    md5_sum(did, strlen(did), _md5);

    char _did_md5[64] = {0x00};
    snprintf(_did_md5, sizeof(_did_md5), "%s#%s", did, _md5);
    #endif
    uint8_t *json_str = (uint8_t *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
    if (!json_str) {
        joyinside_log_err("OOM at alloc joyinside init event buff(2048)");
        return -1;
    }

    joyinside_log_info("did is: %s", did);

    char jwt_data[JWT_BUFFER_SIZE] = {0};
    ret = xiot_jwt_calculation(did, dev_token, jwt_data);
    if (ret != 0) {
        heap_caps_free(json_str);
        return -1;
    }

#ifdef USE_OPUS_ENCODE
    #define AUDIO_FOMART "opus"
#else
    #define AUDIO_FOMART "pcm"
#endif
    //joyinside_log_info("did222 is: %s", _did_md5);
    
    snprintf((char *)json_str, 4096,
            "{"
                "\"mid\":\"%s\","
                "\"contentType\":\"EVENT\","
                "\"content\":{"
                    "\"eventType\":\"CLIENT_VOICE_CHAT_UPDATE\","
                    "\"eventData\":{"
                        "\"audio\":{"
                            "\"binary\": true,"
                            "\"output\":{\"codec\":\"opus\",\"frameSizeMs\":\"60\",\"sampleRate\":\"16000\",\"enableOpusCbr\":true},"
                            "\"input\":{\"codec\":\"%s\",\"sampleRate\":\"16000\"}"
                        "},"
                        "\"features\":{"
                            "\"mcp\":{"
                                "\"serverInfo\":{"
                                    "\"label\":\"jd-iot\","
                                    "\"properties\":{\"did\":\"%s\",\"botId\":\"%s\",\"deviceToken\":\"Bearer %s\"}"
                                "}"
                            "}"
                        "}"
                    "}"
                "}"
            "}",
            msg->mid, AUDIO_FOMART, did, botid, jwt_data);
    printf("Init json_str: %s\n", json_str);
    ret = joyinside_ws_write_text((char *)json_str, strlen((char *)json_str));
    if (ret != 0) {
        heap_caps_free(json_str);
        joyinside_log_err("Failed to send WebSocket message: %d", ret);
        return -1;
    }

    heap_caps_free(json_str);
    return 0;
}

int audio_ping_send(void *ctx, audio_message_t *msg)
{
    uint8_t *json_str = (uint8_t *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (!json_str) {
        return -1;
    }
    snprintf((char *)json_str, 1024, 
        "{\"mid\":\"%s\",\"contentType\":\"PING\"}", 
        msg->mid);

    int ret = joyinside_ws_write_text((char *)json_str, strlen((char *)json_str));
    if (ret < 0) {
        //free(json_str);
        joyinside_log_err("Failed to send WebSocket message: %d", ret);
        ret = -1;
    } else {
        ret = 0;
    }
    heap_caps_free(json_str);
    return ret;
}

int audio_finish_event_send(void *ctx, audio_message_t *msg)
{
    char *json_str = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (!json_str) {
        return -1;
    }
    snprintf((char *)json_str, 1024,
        "{\"mid\": \"%s\",\"contentType\": \"EVENT\","
        "\"content\": {\"eventType\": \"CLIENT_AUDIO_FINISH\"}}", 
        msg->mid);

    int ret = joyinside_ws_write_text((char *)json_str, strlen((char *)json_str));
    if (ret != 0) {
        heap_caps_free(json_str);
        joyinside_log_err("Failed to send WebSocket message: %d", ret);
        return -1;
    }
    heap_caps_free(json_str);
    return 0;
}

int audio_interrupt_send(void *ctx, audio_message_t *msg)
{
    char *json_str = (char *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (!json_str) {
        return -1;
    }
        
    snprintf((char *)json_str, 1024,
                "{\"mid\":\"%s\",\"contentType\":\"EVENT\","
                    "\"content\":{\"eventType\":\"CLIENT_INTERRUPT\"}}", msg->mid);
    printf("interrup message: %s\n", json_str);
    int ret = joyinside_ws_write_text(json_str, strlen((char *)json_str));
    if (ret != 0) {
        heap_caps_free(json_str);
        joyinside_log_err("Failed to send WebSocket message: %d", ret);
        return -1;
    }
    heap_caps_free(json_str);
    return 0;
}

int audio_text_input_send(audio_message_t *msg, char *text)
{
    if (!msg || !text) {
        return -1;
    }
    char *json_str = (char *)heap_caps_malloc(256+strlen(text), MALLOC_CAP_SPIRAM);
    if (!json_str) {
        return -1;
    }
        
    snprintf((char *)json_str, 256+strlen(text),
                "{\"mid\":\"%s\",\"contentType\":\"TEXT\","
                    "\"content\":{\"input\":\"%s\"}}", msg->mid, text);
    printf("input text message: %s\n", json_str);
    int ret = joyinside_ws_write_text(json_str, strlen((char *)json_str));
    if (ret != 0) {
        joyinside_log_err("Failed to send WebSocket message: %d", ret);
        ret = -1;
    } else {
        ret = 0;
    }
    heap_caps_free(json_str);

    return ret;
}

int audio_text_to_speech(audio_message_t *msg, char *text)
{
    if (!msg || !text) {
        return -1;
    }
    char *json_str = (char *)heap_caps_malloc(256+strlen(text), MALLOC_CAP_SPIRAM);
    if (!json_str) {
        return -1;
    }
        
    snprintf((char *)json_str, 256+strlen(text),
                "{\"mid\":\"%s\",\"contentType\":\"EVENT\","
                    "\"content\":{\"eventType\":\"CLIENT_INPUT_TEXT_TO_SPEECH\","
                    "\"eventData\": {\"text\":\"%s\"}}}", msg->mid, text);
    printf("text to speech message: %s\n", json_str);
    int ret = joyinside_ws_write_text(json_str, strlen((char *)json_str));
    if (ret != 0) {
        joyinside_log_err("Failed to send WebSocket message: %d", ret);
        ret = -1;
    } else {
        ret = 0;
    }
    heap_caps_free(json_str);

    return ret;
}

void audio_message_reset(audio_message_t *msg)
{
    msg->index = 0;
    uuid_generate(msg->mid);
}

void audio_message_free(audio_message_t *msg) {
    if (msg) {
        heap_caps_free(msg);
    }
}