

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_heap_caps.h"

#include "mbedtls/base64.h"

//#include "opus.h"

#include "esp_websocket_client.h"
#include "ws_params.h"
#include "joyinside_err.h"
#include "joyinside_log.h"
#include "esp_crt_bundle.h"
#include "ws_connect.h"

#include "audio_message.h"
#include "dev_info.h"

#include "afe.h"
#include "cJSON.h"

//#include "joyinside_player.h"

#include "audio.h"
#include "protocol.h"

#include "joyinside_timer.h"
#include "joyinside_time.h"

#include "opus_encodec_decodec.h"

#define JOYINSDIE_CONNECT_TIMEOUT_MS (5*1000)

#define WS_FRAME_MAX (16*1024)

#define WEBSOCKET_EVENT_ID  (0x01 << 0)
#define AUDIO_SEND_EVENT_ID (0x01 << 1)

#define WSS_DISCONECT_TIMEOUT (30*60*1000)

struct joyinside {
    joyinside_cloud_event_cb event_cb;
    void *user_ptr;
    int status;

    audio_message_t *msg;
    esp_websocket_client_handle_t client;

    char *text_data;
    int text_data_len;

    char botid[64];
    char accesskey_id[64];
    char accesskey_secert[64];
    char device_token[64];

    EventGroupHandle_t event;

    TaskHandle_t send_task_hander;

    int running;

    uint64_t bin_recv_count;
    uint64_t bin_send_count;
};

struct joyinside *_g_joyinside = NULL;

extern bool multi_dialogue_is_enable();
extern void joyinside_cloud_audio_send_stop();
extern bool app_audio_play_mp3(const char *uri);
extern bool app_audio_play_prompt_wav(const char *uri);

extern void set_chat_timer(bool value);

#define T_EVENT                 "EVENT"
#define T_TTS                   "TTS"
#define T_ASR                   "ASR"
#define T_AGENT                 "AGENT"
#define T_ACTIVITY              "ACTIVITY"
#define T_PONG                  "PONG"

#define ET_D_CFG_BOT            "CFG_BOT_EVENT"
#define ET_D_CHAT_UPDATED       "SERVER_VOICE_CHAT_UPDATED"
#define ET_D_AGENT_START        "CALL_AGENT_START_EVENT"
#define ET_D_EMPTY_CONTENT      "EMPTY_CONTENT"
#define ET_D_TTS_SENTENCE_START "TTS_SENTENCE_START"
#define ET_D_TTS_COMPLETE       "TTS_COMPLETE"
#define ET_D_COMPLETE           "COMPLETE"
#define ET_D_INTERRUPTED         "CALL_AGENT_INTERRUPTED"
#define ET_D_VOICE_CHAT_EXIT    "VOICE_CHAT_EXIT_EVENT"
#define ET_D_CALL_INTENT_END    "CALL_INTENT_END_EVENT"

typedef void (*process_func)(cJSON *obj);

typedef struct {
    char *name;
    process_func func;
}process_table_t;

#if 0
extern static void d_event_func(cJSON *content);
extern static void d_tts_func(cJSON *content);
extern static void d_asr_func(cJSON *content);
extern static void d_agent_func(cJSON *content);
extern static void d_activity_func(cJSON *content);
extern static void d_pong_func(cJSON *content);

extern static void e_cfg_func(cJSON *obj);
extern static void e_chat_update_func(cJSON *);
extern static void e_agent_start_func(cJSON *);
extern static void e_empty_content_func(cJSON *);
extern static void e_tts_sentence_start_func(cJSON *);
extern static void e_tts_complete_func(cJSON *);
extern static void e_complete_func(cJSON *);
extern static void e_interrupted_func(cJSON *);
extern static void e_chat_exit_func(cJSON *);
extern static void e_call_end_func(cJSON *);
#endif

char *get_botid()
{
    return _g_joyinside?_g_joyinside->botid:NULL;
}

char *get_dev_token()
{
    return _g_joyinside?_g_joyinside->device_token:NULL;
}

static char *generate_cloud_uri()
{
    char access_sign[33] = {0x00};
    if (!_g_joyinside || 
        (strlen(_g_joyinside->accesskey_id) == 0) || 
        (strlen(_g_joyinside->accesskey_secert) == 0) || 
        (strlen(_g_joyinside->botid) == 0) 
    ) {
        joyinside_log_err("generate_cloud_uri params error.");
        return NULL;
    }
#define CLOUD_VERSION "V2"
    char *uri_params = ws_generate_auth_params(_g_joyinside->accesskey_id, 
            _g_joyinside->accesskey_secert, _g_joyinside->botid, CLOUD_VERSION, access_sign);
    if (!uri_params) {
        joyinside_log_err("ws_generate_auth_params error.\n");
        return NULL;
    }
#define JOYINSIDE_CLOUD_URI "wss://joyinside.jd.com/soulmate/voiceCall/v4\0"
    joyinside_log_debug("uri_params(%d): %s", strlen(uri_params), uri_params);
    int uri_len = strlen(uri_params) + strlen(JOYINSIDE_CLOUD_URI) + 1;
    joyinside_log_debug(">>>>uri len : %d", uri_len);
    char *uri = (char *)malloc(uri_len);
    if (!uri) {
        joyinside_log_err("OOM at alloc joyinside cloud uri params.");
        free(uri_params);
        return NULL;
    }
    memset(uri, 0x00, uri_len);
    snprintf(uri, uri_len, "%s%s", JOYINSIDE_CLOUD_URI, uri_params);
    free(uri_params);

    return uri;
}

static void set_event(int event)
{
    if ( _g_joyinside->event_cb ) {
        _g_joyinside->event_cb(event, _g_joyinside->user_ptr);
    }
}


/* -----------------event message------ */
static void e_cfg_func(cJSON *eventData_obj)
{
    
}

static void e_chat_update_func(cJSON *eventData_obj)
{

}

static void e_agent_start_func(cJSON *eventData_obj)
{

}

static void e_empty_content_func(cJSON *eventData_obj)
{

}

static void e_tts_sentence_start_func(cJSON *eventData_obj)
{
    if (!eventData_obj) {
        joyinside_log_err("TTS_SENTENCE_START not the eventData obj, drop this mesage.");
        return;
    }
    cJSON *text_obj = cJSON_GetObjectItem(eventData_obj, "text");
    if (!text_obj || !cJSON_IsString(text_obj) || text_obj->valuestring == NULL) {
        joyinside_log_warn("The TTS_SENTENCE_START text is empty.");
        return;
    }
    int text_len = strlen(text_obj->valuestring);

    //joyinside_log_debug("TTS_SENTENCE_START text is(%d): %s\n", strlen(text_obj->valuestring), text_obj->valuestring);
    joyinside_log_debug("TTS_SENTENCE_START text is: %s\n", text_obj->valuestring);

    protocol_write_tts_sentence((uint8_t *)text_obj->valuestring, text_len, NULL);
}

static void e_tts_complete_func(cJSON *eventData_obj)
{   
    set_chat_timer(true);
    joyinside_log_debug("e_tts_complete_func..");
    set_event(JOYINSIDE_CLOUD_TTS_COMPLETE);
}

static void e_complete_func(cJSON *eventData_obj)
{

}

static void e_interrupted_func(cJSON *eventData_obj)
{
    set_event(JOYINSIDE_CLOUD_CHAT_INTERRUPT);
}

static void e_chat_exit_func(cJSON *eventData_obj)
{
    set_event(JOYINSIDE_CLOUD_CHAT_EXIT);
}

static void e_call_end_func(cJSON *eventData_obj)
{
    set_event(JOYINSIDE_CLOUD_CHAT_EXIT);
}

const static process_table_t event_process_table[] = {
    {ET_D_CFG_BOT,              e_cfg_func},
    {ET_D_CHAT_UPDATED,         e_chat_update_func},
    {ET_D_AGENT_START,          e_agent_start_func},
    {ET_D_EMPTY_CONTENT,        e_empty_content_func},
    {ET_D_TTS_SENTENCE_START,   e_tts_sentence_start_func},
    {ET_D_TTS_COMPLETE,         e_tts_complete_func},
    {ET_D_COMPLETE,             e_complete_func},
    {ET_D_INTERRUPTED,          e_interrupted_func},
    {ET_D_VOICE_CHAT_EXIT,      e_chat_exit_func},
    {ET_D_CALL_INTENT_END,      e_call_end_func}
};

static void d_event_func(cJSON *content_obj)
{
    if (!content_obj) {
        joyinside_log_err("EVENT message not the content obj, drop this message.");
        return;
    }

    cJSON *type_obj = cJSON_GetObjectItem(content_obj, "eventType");
    if (!type_obj || !cJSON_IsString(type_obj) || type_obj->valuestring == NULL) {
        joyinside_log_debug("EVENT message not find eventType, drop this message.");
        return;
    }
    cJSON *event_data = cJSON_GetObjectItem(content_obj, "eventData");

    int i = 0;    
    for (i = 0; i < sizeof(event_process_table) / sizeof(process_table_t); i++) {
        if ((strlen(event_process_table[i].name) == strlen(type_obj->valuestring)) &&
            (strncmp(type_obj->valuestring, event_process_table[i].name, strlen(event_process_table[i].name)) == 0)) {
                joyinside_log_debug("event %s process.", event_process_table[i].name);
                event_process_table[i].func(event_data);
        }
    }
    return;
}

#define AUDIO_DECODE_SIZE (1024*8)
static unsigned char *__tts_decode_buf = NULL;
static void d_tts_func(cJSON *audio_obj)
{
    if (!audio_obj) {
        joyinside_log_err("TTS not the content obj, drop this message.");
        return;
    }

    cJSON *audioBase64 = cJSON_GetObjectItem(audio_obj, "audioBase64");
    if (!audioBase64 || !cJSON_IsString(audioBase64) || audioBase64->valuestring == NULL) {
        joyinside_log_debug("Not find audioBase64.");
        return;
    }
    if (__tts_decode_buf == NULL) {
        __tts_decode_buf = (unsigned char *)heap_caps_malloc(AUDIO_DECODE_SIZE, MALLOC_CAP_SPIRAM);
        if (!__tts_decode_buf) {
            joyinside_log_err("OOM at alloc decode buffer int PSRAM.");
            return;
        }
        memset(__tts_decode_buf, 0x00, AUDIO_DECODE_SIZE);
    }
   
    int slen = strlen(audioBase64->valuestring);
    size_t dlen = 0;
    //joyinside_log_debug(">>>Start decode src len = %d", slen);
    int ret = mbedtls_base64_decode(__tts_decode_buf, AUDIO_DECODE_SIZE, &dlen, 
            (const unsigned char *)audioBase64->valuestring, slen); 
    if(ret == 0) {
        //joyinside_player_play_opus(__tts_decode_buf, dlen);
        opus_decodec_frame(__tts_decode_buf, dlen);
    }

    return;
}

extern void protocol_write_asr_text(uint8_t *data, uint16_t len, void *user_data);

static void d_asr_func(cJSON *content_obj)
{
    set_chat_timer(true);

    if (!content_obj) {
        joyinside_log_err("ASR not the content obj, drop this message.");
        return;
    }

    cJSON *textType_obj = cJSON_GetObjectItem(content_obj, "textType");
    if (!textType_obj || !cJSON_IsString(textType_obj) || !textType_obj->valuestring) {
        joyinside_log_err("ASR not find the textType filed.");
        return;
    }
    
    cJSON *text_obj = cJSON_GetObjectItem(content_obj, "text");
    if (!text_obj || !cJSON_IsString(text_obj) || !text_obj->valuestring || (strlen(text_obj->valuestring) == 0)) {
        joyinside_log_warn("The ASR text empty.");
        return;
    }

    joyinside_log_info("ASR(%d): %s", strlen(text_obj->valuestring), text_obj->valuestring);
    protocol_write_asr_text((uint8_t *)text_obj->valuestring, strlen(text_obj->valuestring), NULL);

    if (strncmp(textType_obj->valuestring, "IS_FINAL", strlen(textType_obj->valuestring)) == 0) {
        joyinside_log_info("ASR IS_FINAL");
        set_event(JOYINISDE_CLOUD_CHAT_ASR_FINAL);
    }
}

static void d_agent_func(cJSON *content)
{
}

static void d_activity_func(cJSON *content)
{

}

static void d_pong_func(cJSON *content)
{
}

const static process_table_t content_process_table[] = {
    {T_EVENT,       d_event_func},
    {T_TTS,         d_tts_func},
    {T_ASR,         d_asr_func},
    {T_AGENT,       d_agent_func},
    {T_ACTIVITY,    d_activity_func},
    {T_PONG,        d_pong_func},
};

static void cloud_recv_process(cJSON *root)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        printf("cloud recv process:\n%s\n", json);
        cJSON_free(json);
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!code || !cJSON_IsNumber(code)) {
        joyinside_log_debug("Not find code.");
        goto audio_process_end;
    }
    if (code->valueint != 200) {
        joyinside_log_debug("code is't 200, it's: %d", code->valueint);
        goto audio_process_end;
    }
 
    cJSON *content_type_obj = cJSON_GetObjectItem(root, "contentType");
    if (!content_type_obj || !cJSON_IsString(content_type_obj) || content_type_obj->valuestring == NULL) {
        joyinside_log_err("Not find contentType, dorp this message.");
        goto audio_process_end;
    }

    cJSON *content_obj = cJSON_GetObjectItem(root, "content");    
    int i;
    for ( i = 0; i < sizeof(content_process_table) / sizeof(process_table_t); i ++) {
        #if 0
        joyinside_log_debug("content_process_table[%d].name = %s, content_type_obj->valuestring = %s", 
            i, content_process_table[i].name, content_type_obj->valuestring);
        #endif
        if ((strlen(content_process_table[i].name) == strlen(content_type_obj->valuestring)) &&
            (strncmp(content_process_table[i].name, content_type_obj->valuestring, strlen(content_type_obj->valuestring)) == 0)) {
            joyinside_log_debug("content obj %s to process.", content_process_table[i].name);
            content_process_table[i].func(content_obj);
            break;
        }
    }

audio_process_end:

    if (root) {
        cJSON_Delete(root);
    }
}

static void joyinside_ws_data_process(esp_websocket_event_data_t *data)
{
    switch(data->op_code) {
    case 0x00: //continue frame
        break;
    case WS_TRANSPORT_OPCODES_TEXT: //text frame, 0x01
        //WS_TRANSPORT_OPCODES_TEXT
        if (data->payload_len > WS_FRAME_MAX) {
            joyinside_log_warn("The ws binary frame too large(%d > %d), drop it.", data->payload_len, WS_FRAME_MAX);
            return;
        }
        if ((data->data_len + _g_joyinside->text_data_len) >= WS_FRAME_MAX) {
            joyinside_log_warn("The recv json to large, drop some data.");
            _g_joyinside->text_data_len = 0;
            return;
        }

        memcpy(_g_joyinside->text_data + _g_joyinside->text_data_len, data->data_ptr, data->data_len );
        _g_joyinside->text_data_len += data->data_len;
        _g_joyinside->text_data[_g_joyinside->text_data_len] = '\0';

        char *parse_ptr = NULL;
        while (1) {
            cJSON *root_obj = cJSON_ParseWithOpts((const char *)_g_joyinside->text_data, (const char **)&parse_ptr, 1);
            if (root_obj) {
                cloud_recv_process(root_obj);
                _g_joyinside->text_data_len -= (parse_ptr - _g_joyinside->text_data);
                //joyinside_log_debug("<<<<<<buf_ptr = %d\n", buf_ptr);
                if (_g_joyinside->text_data_len > 0) {
                    memmove(_g_joyinside->text_data, parse_ptr, _g_joyinside->text_data_len);
                } else {
                    //joyinside_log_debug(">>>>> data all process.");
                    break;
                }                    
            } else {
                if (_g_joyinside->text_data_len == WS_FRAME_MAX) {
                    _g_joyinside->text_data_len = 0;
                }
                joyinside_log_debug(">>>>> Parse json str is't json");
                break;
            }
        }
        break;
    case WS_TRANSPORT_OPCODES_BINARY: //0x02: //bin frame
        set_chat_timer(true);
        if (data->payload_len > WS_FRAME_MAX) {
            joyinside_log_err("The ws binary frame too large(%d > %d), drop it.", data->payload_len, WS_FRAME_MAX);
            return;
        }
        opus_decodec_frame(data->data_ptr, data->data_len);
        _g_joyinside->bin_recv_count += data->data_len;
        break;        
    case WS_TRANSPORT_OPCODES_PING: //0x09: //ping
        joyinside_log_info("Recv the ping frame.");
        break;
    case WS_TRANSPORT_OPCODES_PONG: //0x0A: //pong
        joyinside_log_info("Recv the pong frame.");
        break;
    case 0x08://close
        joyinside_log_info("Recv the joyinside close frame.");
        break;
    default:

        break;
    }
}

extern void joyinside_cloud_audio_send_start();
extern void joyinside_cloud_audio_send_stop();

static void joyinside_event_handler(void *handler_args, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            joyinside_log_info("WebSocket已连接");
            set_event(JOYINSIDE_CLOUD_CLIENT_EVENT_CONNECTED);
            break;
         case WEBSOCKET_EVENT_DATA:
            joyinside_ws_data_process(data);
            break;

            
        case WEBSOCKET_EVENT_ERROR:
            joyinside_log_err("WebSocket erroe event");
            joyinside_log_err("SSL err: %d", data->error_handle.esp_tls_last_esp_err);
            joyinside_log_err("SSL mbedtls err: %d", data->error_handle.esp_tls_stack_err);
            joyinside_log_err("SSL mbedtls flags: 0x%x", data->error_handle.esp_tls_cert_verify_flags);

            set_event(JOYINSIDE_CLOUD_CLIENT_EVENT_ERROR);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            joyinside_log_info("WebSocket disconnect");
            
        case WEBSOCKET_EVENT_CLOSED:
            joyinside_log_info("WebSocket cloesed.");
        case WEBSOCKET_EVENT_FINISH:
            joyinside_log_info("WebSocket client finished.");
            set_event(JOYINSIDE_CLOUD_CLIENT_EVENT_DISCONNECTED);
        default:
            break;
    }
}

int joyinside_cloud_connect()
{
    if (!_g_joyinside || !_g_joyinside->client) {
        return JOYINSIDE_ERR_NOT_INIT;
    }

    if ( joyinside_cloud_is_conneced() ) {
        return JOYINSIDE_ERR_BUSY;
    }

    if (strlen(_g_joyinside->accesskey_id) == 0) {
        if ((get_info_by_key(JOYINSIDE_KV_JOYINSIDE_ACCESSKEY_ID, 
            _g_joyinside->accesskey_id, sizeof(_g_joyinside->accesskey_id)) != JOYINSIDE_OK) &&
            (strlen(_g_joyinside->accesskey_id) == 0)) {
            joyinside_log_err("Get acceskey_id failed.");
            return JOYINSIDE_ERR_INVALID_ARG;
        }
    }

    if (strlen(_g_joyinside->accesskey_secert) == 0) {
        if ((get_info_by_key(JOYINSIDE_KV_JOYINSIDE_ACCESSKEY_SECERT, 
                _g_joyinside->accesskey_secert, sizeof(_g_joyinside->accesskey_secert)) != JOYINSIDE_OK) &&
            (strlen(_g_joyinside->accesskey_secert) == 0)) {
            joyinside_log_err("Get accesskey_secert failed.");
            return JOYINSIDE_ERR_INVALID_ARG;
        }
    }

    if (strlen(_g_joyinside->botid) == 0) {
        if ((get_info_by_key(JOYINSIDE_KV_JOYINSIDE_BOTID, 
                _g_joyinside->botid, sizeof(_g_joyinside->botid)) != JOYINSIDE_OK) &&
            (strlen(_g_joyinside->botid) == 0)){
            joyinside_log_err("Get botid failed.");
            return JOYINSIDE_ERR_INVALID_ARG;
        }
    }

    if (strlen(_g_joyinside->device_token) == 0) {
        if ((get_info_by_key(JOYINSIDE_KV_DEVICETOKEN, 
                _g_joyinside->device_token, sizeof(_g_joyinside->device_token)) != JOYINSIDE_OK) &&
            (strlen(_g_joyinside->device_token) == 0)){
            joyinside_log_err("Get device_token failed.");
            return JOYINSIDE_ERR_INVALID_ARG;
        }
    }

    char *uri = generate_cloud_uri();
    if (!uri) {
        return JOYINSIDE_ERR_INVALID_ARG;
    }
    
    if (esp_websocket_client_set_uri(_g_joyinside->client, uri ) != ESP_OK) {
        free(uri);
        return JOYINSIDE_ERR_INVALID_ARG;
    }

    // 启动WebSocket客户端
    if (esp_websocket_client_start(_g_joyinside->client) != ESP_OK) {
        free(uri);
        return JOYINSIDE_ERR_FAIL;
    }
    free(uri);

    joyinside_log_info("WebSocket客户端初始化成功");

    set_event(JOYINSIDE_CLOUD_CLIENT_EVENT_CONNETTINT);

    return JOYINSIDE_ERR_OK;
}

int joyinside_cloud_disconnect()
{   
    if ( !_g_joyinside || !_g_joyinside->client ) {
        return JOYINSIDE_ERR_FAIL;
    }
    opus_encode_stop();
    joyinside_cloud_audio_send_stop();
    //opus_encodec_queue_reset();
    esp_websocket_client_close(_g_joyinside->client, pdMS_TO_TICKS(5));
    return JOYINSIDE_ERR_OK;
}

int joyinside_cloud_is_conneced()
{
    if (_g_joyinside && _g_joyinside->client && esp_websocket_client_is_connected(_g_joyinside->client)) {
        return 1;
    }
    return 0;
}

int joyinside_cloud_chat_update()
{
    return audio_init_event_send(NULL, _g_joyinside->msg);
}

int joyinside_cloud_chat_reset()
{
    audio_message_reset(_g_joyinside->msg);
    return 0;
}

void joyinside_cloud_deinit()
{
    if (!_g_joyinside) {
        return;
    }
    joyinside_cloud_disconnect(1, -1);

    if (_g_joyinside->text_data) {
        free(_g_joyinside->text_data);
    }

    if (_g_joyinside->client) {
        esp_websocket_client_destroy(_g_joyinside->client);
        free(_g_joyinside->client);
        _g_joyinside->client = NULL;
    }

    free(_g_joyinside);
    _g_joyinside = NULL;

}

int joyinside_ws_write_text(unsigned char *text, int len)
{
    int r = 0, s = 0;
    while (s < len) {
        r = esp_websocket_client_send_text(_g_joyinside->client, (const char *)text+s, len-s, pdMS_TO_TICKS(6000));
        if (r <= 0) {
            break;
        }
        s += r;
    }
    if ( s == len ) {
        return JOYINSIDE_OK;
    }
    return JOYINSIDE_ERR_FAIL;
}

int joyinside_ws_write_bin(unsigned char *bin, int len)
{
    int r = 0, s = 0;
    while (s < len) {
        r = esp_websocket_client_send_bin(_g_joyinside->client, (const char *)bin+s, len-s, pdMS_TO_TICKS(6000));
        if (r <= 0) {
            break;
        }
        s += r;
    }
    if ( s == len ) {
        return JOYINSIDE_OK;
    }
    return JOYINSIDE_ERR_FAIL;
}

#define OPUS_FRAME_MS 60
#define AUDIO_S_RATE 16000
#define AUDIO_S_CHANNEL 1
#define AUDIO_S_BIT 16
#define AUDIO_S_FRAME_SIZE (AUDIO_S_RATE * OPUS_FRAME_MS / 1000 * 2)

#define OPUS_BUFF_SIZE (512)

void joyinside_cloud_audio_send_start()
{
    if(_g_joyinside && _g_joyinside->event) {
        xEventGroupSetBits(_g_joyinside->event, AUDIO_SEND_EVENT_ID);
    }
}

void joyinside_cloud_audio_send_stop()
{
    if (_g_joyinside && _g_joyinside->event) {
        xEventGroupClearBits(_g_joyinside->event, AUDIO_SEND_EVENT_ID);
    }
}

#define OPUS_FRAM_MAX (512)
extern QueueHandle_t opus_get_encodec_queue();

static void joyinside_cloud_send_task(void *arg)
{
    unsigned char* opus = (unsigned char *)heap_caps_malloc(OPUS_FRAM_MAX, MALLOC_CAP_SPIRAM);
    if (!opus) {
        joyinside_log_err("OOM at alloc opus buff.");
        goto joyinside_cloud_send_task_end;
    }

    int err = 0;
    uint64_t send = 0;
    QueueHandle_t send_queue = NULL;
    while ( _g_joyinside->running ) {
        send_queue = opus_get_encodec_queue();
        if (!send_queue) {
            vTaskDelay(pdMS_TO_TICKS(1000*10));
            continue;
        }  else {
            break;
        }
    }
    int olen = 0;
    while (_g_joyinside->running ) {

        xEventGroupWaitBits(_g_joyinside->event, 
                AUDIO_SEND_EVENT_ID, pdFALSE, pdFALSE, portMAX_DELAY);

        if (!_g_joyinside->running) {
            break;
        }
        if (!joyinside_cloud_is_conneced()) {
            xEventGroupClearBits(_g_joyinside->event, AUDIO_SEND_EVENT_ID);
            continue;
        }

        if ( xQueueReceive(send_queue, opus, portMAX_DELAY) == pdPASS) {
            olen = *(int*)opus;
#if 1
            if (joyinside_cloud_is_conneced() && joyinside_ws_write_bin(opus+sizeof(int), olen) == JOYINSIDE_OK) {
                 _g_joyinside->bin_send_count += olen;
                if (send++ % 100 == 0) {
                    joyinside_log_debug("Ws send audio bin frame: %llu", send);
                }
            } else {
                if ( !joyinside_cloud_is_conneced() ) {
                    xEventGroupClearBits(_g_joyinside->event, AUDIO_SEND_EVENT_ID);
                    //esp_websocket_client_stop();
                }
                joyinside_log_warn("ws bin send failed. drop the opus data.");
            } 
#else
        if (audio_message_send(NULL, _g_joyinside->msg, opus+sizeof(int), (size_t)*(int*)opus) < 0) {
            joyinside_log_warn("audio_message_send failed, drop the opus data.");
            continue;
        } 
#endif
           
        } else {
            xEventGroupClearBits(_g_joyinside->event, AUDIO_SEND_EVENT_ID);
        }
    }

joyinside_cloud_send_task_end:
    if (opus) {
        heap_caps_free(opus);
        opus = NULL;
    }

    joyinside_log_info("ws audio send task stop.");
    vTaskDelete(NULL);
}


void joyinside_cloud_chat_interrupt()
{
    if (joyinside_cloud_is_conneced()) {
        audio_interrupt_send(NULL, _g_joyinside->msg);
    }
}

void joyinside_cloud_text_input(char *text)
{

}


int joyinside_cloud_text_to_speech(char *text)
{
    if (! joyinside_cloud_is_conneced()) {
        return -1;
    }    
    joyinside_cloud_chat_interrupt();
    app_audio_play_reset();
    //audio_message_reset(_g_joyinside->msg);

    if (audio_text_to_speech(_g_joyinside->msg, text) == 0) {
        return 0;
    } 
    return -1;
}


int joyinside_cloud_init(joyinside_cloud_event_cb statuscb, void *user_ptr)
{   
    if (_g_joyinside) {
        return JOYINSIDE_ERR_FAIL;
    }
    _g_joyinside = (struct joyinside *)malloc(sizeof(struct joyinside));
    if (!_g_joyinside) {
        joyinside_log_err("OOM at alloc joyinside handler.");
        return JOYINSIDE_ERR_NO_MEM;
    }
    memset(_g_joyinside, 0x00, sizeof(struct joyinside));

    _g_joyinside->event_cb = statuscb;
    _g_joyinside->user_ptr = user_ptr;

    esp_websocket_client_config_t ws_cfg = {
        .task_prio = 5,
        .task_stack = 6*1024,
        .buffer_size = 16384,
        .ping_interval_sec = 10,
        .pingpong_timeout_sec = 30,
        .disable_auto_reconnect = true,
        .network_timeout_ms = JOYINSDIE_CONNECT_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        //.transport = WEBSOCKET_TRANSPORT_OVER_SSL,
    };

    _g_joyinside->client = esp_websocket_client_init(&ws_cfg);
    if (!_g_joyinside->client) {
        joyinside_log_err("Create joyinside websocket error.");
        joyinside_cloud_deinit();
        return JOYINSIDE_ERR_FAIL;
    }

    _g_joyinside->text_data = heap_caps_malloc(WS_FRAME_MAX, MALLOC_CAP_SPIRAM);
    if (!_g_joyinside->text_data) {
        joyinside_cloud_deinit();
        joyinside_log_err("OOM at alloc text buffer");
        joyinside_cloud_deinit();
        return JOYINSIDE_ERR_FAIL;

    }

    if (!_g_joyinside->msg) {
        _g_joyinside->msg = audio_message_create(global_get_did());
        if (!_g_joyinside->msg) {
            joyinside_log_err("audio_message_create failed.\n");
            joyinside_cloud_deinit();
            return JOYINSIDE_ERR_FAIL;
        }
    }
    
    // 注册事件处理函数
    if (esp_websocket_register_events(_g_joyinside->client, WEBSOCKET_EVENT_ANY, joyinside_event_handler, NULL) != ESP_OK) {
        joyinside_cloud_deinit();
        return JOYINSIDE_ERR_FAIL;
    }

    _g_joyinside->event = xEventGroupCreate();
    if (!_g_joyinside->event) {
        joyinside_log_err("xEventGroupCreate player event failed.");
        joyinside_cloud_deinit();
        return JOYINSIDE_ERR_FAIL;
    }
    //xEventGroupSetBits(_g_joyinside->event, AUDIO_SEND_EVENT_ID);


#define AI_SEND_TASK_STACK (8*1024)
    _g_joyinside->running = 1;
    int ret = xTaskCreatePinnedToCoreWithCaps(joyinside_cloud_send_task, "audio_s", 
            AI_SEND_TASK_STACK, NULL, 9, &_g_joyinside->send_task_hander, tskNO_AFFINITY, MALLOC_CAP_SPIRAM);
    if ( ret != pdPASS || !_g_joyinside->send_task_hander ) {
        joyinside_log_err("xTaskCreateStaticPinnedToCore send stask error.");
        joyinside_cloud_deinit();
        return JOYINSIDE_ERR_FAIL;
    }

    #if 0
    _g_joyinside->disconnect_timer = joyinside_timer_create("ws_disc", 0, joyinside_cloud_timeout, NULL);
    if (!_g_joyinside->disconnect_timer) {
        joyinside_log_err("joyinside_timer_create disconnect timer failed.");
        joyinside_cloud_deinit();
        return JOYINSIDE_ERR_FAIL;
    }
    _g_joyinside->disconnect_timeout = WSS_DISCONECT_TIMEOUT;
    get_info_by_key(JOYINSIDE_KV_JOYINSIDE_TALK_TIMEOUT, 
            (char *)&_g_joyinside->disconnect_timeout, 
            sizeof(_g_joyinside->disconnect_timeout));
    #endif

    set_event(JOYINSIDE_CLOUD_CLIENT_EVENT_INIT);
    return JOYINSIDE_ERR_OK;
}

