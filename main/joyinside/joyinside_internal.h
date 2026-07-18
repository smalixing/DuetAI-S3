/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JOYINSIDE_INTERNAL_H__
#define __JOYINSIDE_INTERNAL_H__

#include <stdbool.h>
#include <stdint.h>

#include "joyinside.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented in joyinside.c, called from the protocol dispatcher. */

/**
 * @brief  Forward a status event to the user event callback
 */
void ji_emit_event(joyinside_handle_t handle, joyinside_event_t event);

/**
 * @brief  Forward an ASR / TTS text line to the user text callback
 */
void ji_emit_text(joyinside_handle_t handle, bool is_asr, const char *text);

/**
 * @brief  Decode a base64 opus payload (from a TTS JSON frame) and emit PCM
 */
void ji_emit_tts_base64(joyinside_handle_t handle, const char *audio_base64);

/**
 * @brief  Forward a server AUDIO_BOOK_PLAY request to the user audiobook callback
 */
void ji_emit_audiobook(joyinside_handle_t handle, const joyinside_audiobook_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* __JOYINSIDE_INTERNAL_H__ */
