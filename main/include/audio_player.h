/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include <stdint.h>

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

/**
 * @brief Begin a streaming PCM playback session.
 *
 * Sets the codec sample rate, enables the power amplifier, and unmutes the DAC.
 * Follow with one or more audio_player_pcm_write() calls, then audio_player_pcm_end().
 *
 * @param sample_rate PCM sample rate in Hz (e.g. 16000)
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_STATE: audio_player not initialized
 */
esp_err_t audio_player_pcm_begin(uint32_t sample_rate);

/**
 * @brief Write one 16-bit mono PCM frame to the codec DAC.
 *
 * Duplicates each mono sample to the L/R channels. Blocks until the frame is
 * queued to the I2S DMA. Must be called between audio_player_pcm_begin() and
 * audio_player_pcm_end().
 *
 * @param pcm     16-bit mono PCM samples
 * @param samples Number of int16_t samples in pcm
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: pcm is NULL or samples <= 0
 *    - ESP_ERR_NO_MEM: Scratch buffer allocation failed
 */
esp_err_t audio_player_pcm_write(const int16_t *pcm, int samples);

/**
 * @brief End a streaming PCM playback session.
 *
 * Flushes the I2S DMA buffer and mutes the DAC to avoid idle hiss.
 */
void audio_player_pcm_end(void);

#ifdef __cplusplus
}
#endif
