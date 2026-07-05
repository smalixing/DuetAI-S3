/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount the "voice" SPIFFS partition holding WAV prompt files.
 *
 * Must be called once after the board (I2S + codec) is initialized and
 * before any audio_player_play_file() call.
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t audio_player_init(void);

/**
 * @brief Play a 16 kHz / 16-bit / mono PCM WAV file over the codec DAC.
 *
 * Blocks until playback finishes. Safe to call from a task context
 * (e.g. the wake word callback runs on the wake word task).
 *
 * @param path Absolute VFS path, e.g. "/voice/I_comeon.wav"
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NOT_FOUND: File not found
 *    - Others: Fail
 */
esp_err_t audio_player_play_file(const char *path);

#ifdef __cplusplus
}
#endif
