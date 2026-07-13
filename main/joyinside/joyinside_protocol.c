/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "hal_log.h"

#include "joyinside_protocol.h"
#include "joyinside_internal.h"

/* contentType values */
#define CT_EVENT                    "EVENT"
#define CT_TTS                      "TTS"
#define CT_ASR                      "ASR"
#define CT_AGENT                    "AGENT"
#define CT_ACTIVITY                 "ACTIVITY"
#define CT_PONG                     "PONG"

/* eventType values */
#define ET_TTS_SENTENCE_START       "TTS_SENTENCE_START"
#define ET_TTS_COMPLETE             "TTS_COMPLETE"
#define ET_INTERRUPTED              "CALL_AGENT_INTERRUPTED"
#define ET_VOICE_CHAT_EXIT          "VOICE_CHAT_EXIT_EVENT"
#define ET_CALL_INTENT_END          "CALL_INTENT_END_EVENT"

#define ASR_TEXT_TYPE_FINAL         "IS_FINAL"

typedef void (*content_handler_t)(joyinside_handle_t handle, cJSON *content);
typedef void (*event_handler_t)(joyinside_handle_t handle, cJSON *event_data);

typedef struct {
    const char       *type;
    content_handler_t handler;
} content_entry_t;

typedef struct {
    const char     *type;
    event_handler_t handler;
} event_entry_t;

static const char *_json_str(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsString(item) || item->valuestring == NULL) {
        return NULL;
    }
    return item->valuestring;
}

/* ---------------- event (EVENT contentType) handlers ---------------- */

static void _ev_tts_sentence_start(joyinside_handle_t handle, cJSON *event_data)
{
    const char *text = _json_str(event_data, "text");
    if (text == NULL) {
        hal_log_warn("TTS_SENTENCE_START: empty text");
        return;
    }
    ji_emit_text(handle, false, text);
}

static void _ev_tts_complete(joyinside_handle_t handle, cJSON *event_data)
{
    (void)event_data;
    ji_emit_event(handle, JOYINSIDE_EVENT_TTS_COMPLETE);
}

static void _ev_interrupted(joyinside_handle_t handle, cJSON *event_data)
{
    (void)event_data;
    ji_emit_event(handle, JOYINSIDE_EVENT_INTERRUPTED);
}

static void _ev_chat_exit(joyinside_handle_t handle, cJSON *event_data)
{
    (void)event_data;
    ji_emit_event(handle, JOYINSIDE_EVENT_CHAT_EXIT);
}

static const event_entry_t EVENT_TABLE[] = {
    {ET_TTS_SENTENCE_START, _ev_tts_sentence_start},
    {ET_TTS_COMPLETE,       _ev_tts_complete},
    {ET_INTERRUPTED,        _ev_interrupted},
    {ET_VOICE_CHAT_EXIT,    _ev_chat_exit},
    {ET_CALL_INTENT_END,    _ev_chat_exit},
};

/* ---------------- contentType handlers ---------------- */

static void _ct_event(joyinside_handle_t handle, cJSON *content)
{
    const char *event_type = _json_str(content, "eventType");
    if (event_type == NULL) {
        hal_log_debug("EVENT: no eventType, drop");
        return;
    }
    cJSON *event_data = cJSON_GetObjectItem(content, "eventData");

    for (size_t i = 0; i < sizeof(EVENT_TABLE) / sizeof(EVENT_TABLE[0]); i++) {
        if (strcmp(event_type, EVENT_TABLE[i].type) == 0) {
            EVENT_TABLE[i].handler(handle, event_data);
            return;
        }
    }
}

static void _ct_tts(joyinside_handle_t handle, cJSON *content)
{
    const char *audio_base64 = _json_str(content, "audioBase64");
    if (audio_base64 == NULL) {
        hal_log_debug("TTS: no audioBase64");
        return;
    }
    ji_emit_tts_base64(handle, audio_base64);
}

static void _ct_asr(joyinside_handle_t handle, cJSON *content)
{
    const char *text = _json_str(content, "text");
    const char *text_type = _json_str(content, "textType");
    if (text == NULL || text[0] == '\0') {
        hal_log_warn("ASR: empty text");
        return;
    }

    ji_emit_text(handle, true, text);

    if (text_type && strcmp(text_type, ASR_TEXT_TYPE_FINAL) == 0) {
        ji_emit_event(handle, JOYINSIDE_EVENT_ASR_FINAL);
    } else {
        ji_emit_event(handle, JOYINSIDE_EVENT_ASR);
    }
}

static const content_entry_t CONTENT_TABLE[] = {
    {CT_EVENT, _ct_event},
    {CT_TTS,   _ct_tts},
    {CT_ASR,   _ct_asr},
};

void joyinside_protocol_dispatch(joyinside_handle_t handle, cJSON *root)
{
    if (root == NULL) {
        return;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code && cJSON_IsNumber(code) && code->valueint != 200) {
        hal_log_debug("Dispatch: server code=%d, drop", code->valueint);
        return;
    }

    const char *content_type = _json_str(root, "contentType");
    if (content_type == NULL) {
        hal_log_warn("Dispatch: no contentType, drop");
        return;
    }

    cJSON *content = cJSON_GetObjectItem(root, "content");
    for (size_t i = 0; i < sizeof(CONTENT_TABLE) / sizeof(CONTENT_TABLE[0]); i++) {
        if (strcmp(content_type, CONTENT_TABLE[i].type) == 0) {
            CONTENT_TABLE[i].handler(handle, content);
            break;
        }
    }
}

/* ---------------- outbound message builders ---------------- */

int joyinside_protocol_build_chat_update(char *buf, int buf_len, const char *mid,
                                         const char *device_id, const char *bot_id,
                                         const char *jwt)
{
    int n = snprintf(buf, buf_len,
        "{"
            "\"mid\":\"%s\","
            "\"contentType\":\"EVENT\","
            "\"content\":{"
                "\"eventType\":\"CLIENT_VOICE_CHAT_UPDATE\","
                "\"eventData\":{"
                    "\"audio\":{"
                        "\"binary\": true,"
                        "\"output\":{\"codec\":\"opus\",\"frameSizeMs\":\"60\",\"sampleRate\":\"16000\",\"enableOpusCbr\":true},"
                        "\"input\":{\"codec\":\"opus\",\"sampleRate\":\"16000\"},"
                        "\"timbre\":{\"voiceVolume\":\"50\"}"
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
        mid, device_id, bot_id, jwt);
    if (n <= 0 || n >= buf_len) {
        hal_log_err("Build chat-update failed: buffer too small");
        return -1;
    }
    return n;
}

int joyinside_protocol_build_interrupt(char *buf, int buf_len, const char *mid)
{
    int n = snprintf(buf, buf_len,
        "{\"mid\":\"%s\",\"contentType\":\"EVENT\","
        "\"content\":{\"eventType\":\"CLIENT_INTERRUPT\"}}", mid);
    if (n <= 0 || n >= buf_len) {
        hal_log_err("Build interrupt failed: buffer too small");
        return -1;
    }
    return n;
}

int joyinside_protocol_build_audio_finish(char *buf, int buf_len, const char *mid)
{
    int n = snprintf(buf, buf_len,
        "{\"mid\":\"%s\",\"contentType\":\"EVENT\","
        "\"content\":{\"eventType\":\"CLIENT_AUDIO_FINISH\"}}", mid);
    if (n <= 0 || n >= buf_len) {
        hal_log_err("Build audio-finish failed: buffer too small");
        return -1;
    }
    return n;
}

int joyinside_protocol_build_tts(char *buf, int buf_len, const char *mid, const char *text)
{
    int n = snprintf(buf, buf_len,
        "{\"mid\":\"%s\",\"contentType\":\"EVENT\","
        "\"content\":{\"eventType\":\"CLIENT_INPUT_TEXT_TO_SPEECH\","
        "\"eventData\": {\"text\":\"%s\"}}}", mid, text);
    if (n <= 0 || n >= buf_len) {
        hal_log_err("Build tts failed: buffer too small");
        return -1;
    }
    return n;
}

int joyinside_protocol_build_ping(char *buf, int buf_len, const char *mid)
{
    int n = snprintf(buf, buf_len, "{\"mid\":\"%s\",\"contentType\":\"PING\"}", mid);
    if (n <= 0 || n >= buf_len) {
        hal_log_err("Build ping failed: buffer too small");
        return -1;
    }
    return n;
}
