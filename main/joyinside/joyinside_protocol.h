/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JOYINSIDE_PROTOCOL_H__
#define __JOYINSIDE_PROTOCOL_H__

#include "joyinside.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Dispatch one parsed inbound JSON message
 *
 *         Translates JoyInside contentType/eventType messages into the public
 *         event / text / PCM callbacks via the joyinside_internal.h hooks.
 *
 * @param[in]  handle  Client handle passed back to the callbacks
 * @param[in]  root    Parsed JSON object; borrowed (caller still owns and frees it)
 */
void joyinside_protocol_dispatch(joyinside_handle_t handle, cJSON *root);

/**
 * @brief  Build the CLIENT_VOICE_CHAT_UPDATE event JSON that starts a turn
 *
 * @param[out]  buf       Output buffer
 * @param[in]   buf_len   Size of buf
 * @param[in]   mid       Message id (UUID)
 * @param[in]   device_id Device id (did)
 * @param[in]   bot_id    Bot id
 * @param[in]   jwt       Device JWT token
 *
 * @return  Bytes written (excluding NUL), or -1 on truncation
 */
int joyinside_protocol_build_chat_update(char *buf, int buf_len, const char *mid,
                                         const char *device_id, const char *bot_id,
                                         const char *jwt);

/**
 * @brief  Build a CLIENT_INTERRUPT event JSON
 */
int joyinside_protocol_build_interrupt(char *buf, int buf_len, const char *mid);

/**
 * @brief  Build a CLIENT_AUDIO_FINISH event JSON
 *
 *         Signals to the server that the client has finished sending uplink
 *         audio for the current turn, so it can run recognition and respond.
 */
int joyinside_protocol_build_audio_finish(char *buf, int buf_len, const char *mid);

/**
 * @brief  Build a CLIENT_INPUT_TEXT_TO_SPEECH event JSON
 */
int joyinside_protocol_build_tts(char *buf, int buf_len, const char *mid, const char *text);

/**
 * @brief  Build a PING message JSON
 */
int joyinside_protocol_build_ping(char *buf, int buf_len, const char *mid);

/**
 * @brief  Build a TEXT input message JSON (contentType "TEXT")
 *
 *         Sends a typed user turn to the server as text instead of audio; the
 *         server treats it like a recognized utterance and responds normally.
 *
 * @param[out]  buf      Output buffer
 * @param[in]   buf_len  Size of buf
 * @param[in]   mid      Message id (UUID)
 * @param[in]   text     NUL-terminated UTF-8 user input text
 *
 * @return  Bytes written (excluding NUL), or -1 on truncation
 */
int joyinside_protocol_build_text_input(char *buf, int buf_len, const char *mid, const char *text);

/**
 * @brief  Build an uplink AUDIO_BOOK_PLAY message JSON
 *
 *         Reports to the server that the client has started playing a chapter.
 */
int joyinside_protocol_build_audiobook_play(char *buf, int buf_len, const char *mid,
                                            const char *book_id, const char *chapter_id);

/**
 * @brief  Build an uplink AUDIO_BOOK_STOP message JSON
 *
 *         Reports that playback stopped. On natural completion pass finish=true
 *         with progress set to the played length so the server can auto-advance.
 */
int joyinside_protocol_build_audiobook_stop(char *buf, int buf_len, const char *mid,
                                            const char *book_id, const char *chapter_id,
                                            long progress, bool finish);

/**
 * @brief  Build an uplink AUDIO_BOOK_PING (keepalive) message JSON
 */
int joyinside_protocol_build_audiobook_ping(char *buf, int buf_len, const char *mid);

#ifdef __cplusplus
}
#endif

#endif /* __JOYINSIDE_PROTOCOL_H__ */
