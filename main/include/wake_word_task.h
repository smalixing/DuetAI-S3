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
 * @brief Wake word detection callback function type
 * 
 * @param wake_word_index The index of detected wake word (1-based)
 * @param wake_word_name The name of detected wake word
 */
typedef void (*wake_word_cb_t)(int wake_word_index, const char *wake_word_name);

/**
 * @brief Captured mono PCM callback function type
 *
 * Invoked from the wake word task for every audio chunk read from I2S,
 * regardless of detection state. Keep it short and non-blocking (enqueue or
 * accumulate only); it runs on the detection hot path.
 *
 * @param pcm     16 kHz 16-bit mono PCM samples (valid only during the call)
 * @param samples Number of int16_t samples in pcm
 */
typedef void (*wake_word_pcm_cb_t)(const int16_t *pcm, int samples);

/**
 * @brief Initialize and start wake word detection task
 * 
 * This function initializes the wakenet model, I2S audio interface,
 * and creates a FreeRTOS task to continuously detect wake words.
 * 
 * @param callback Callback function called when wake word is detected
 * @return 
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: Memory allocation failed
 *    - ESP_ERR_NOT_FOUND: Wakenet model not found
 *    - Others: Fail
 */
esp_err_t wake_word_task_start(wake_word_cb_t callback);

/**
 * @brief Stop wake word detection task
 * 
 * @return 
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t wake_word_task_stop(void);

/**
 * @brief Register a callback to receive captured mono PCM.
 *
 * The callback is invoked for every I2S audio chunk from the detection loop.
 * Pass NULL to detach. Safe to call while the task is running.
 *
 * @param cb Callback to receive PCM, or NULL to disable forwarding
 */
void wake_word_task_set_pcm_callback(wake_word_pcm_cb_t cb);

#ifdef __cplusplus
}
#endif