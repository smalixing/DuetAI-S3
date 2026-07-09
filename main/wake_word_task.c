/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_log.h"

#include "wake_word_task.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"

#include "bsp_board.h"
#include "bsp_i2s.h"

static const char *TAG = "wake_word";

// Wake word detection task configuration
// esp-sr wakenet needs a generous stack, especially on the detection branch
// (get_triggered_channel + callback + clean). 4 KB overflows and crashes with
// a LoadProhibited panic right after the first detection. The callback also
// plays a WAV prompt (SPIFFS file I/O + i2s_write) on this task, so keep it big.
#define WAKE_WORD_TASK_STACK_SIZE   (12 * 1024)
#define WAKE_WORD_TASK_PRIORITY     (5)
#define WAKE_WORD_TASK_CORE_ID      (1)

// SPIFFS partition holding the packed srmodels.bin (see partitions.csv)
#define WAKE_WORD_MODEL_PARTITION   "model"

// Wake word model (你好东东 / SR_WN_WN9_NIHAODONGDONG_TTS2)
#define WAKE_WORD_MODEL_NAME        "nihaodongdong"

// The ES7210 streams its ADCs as interleaved I2S slots. The bus is configured
// as 16-bit stereo (I2S_CHANNEL_FMT_RIGHT_LEFT), so every I2S frame carries
// two interleaved samples (ADC1 = MIC1, ADC2 = MIC2 on SDOUT1). The wakenet
// model expects a single mono stream, so we grab one capture channel.
#define WAKE_WORD_I2S_CHANNELS      (2)
// Which interleaved slot to feed the model (0 = MIC1, 1 = MIC2)
#define WAKE_WORD_CAPTURE_CHANNEL   (0)

// Task handle
static TaskHandle_t s_wake_word_task_handle = NULL;
static volatile bool s_task_running = false;

// Wake word model data
static srmodel_list_t *s_models = NULL;
static model_iface_data_t *s_model_data = NULL;
static const esp_wn_iface_t *s_wakenet_iface = NULL;
static char *s_wake_word_name = NULL;

// Callback
static wake_word_cb_t s_wake_word_callback = NULL;
static wake_word_pcm_cb_t s_pcm_callback = NULL;

/**
 * @brief Wake word detection task
 * 
 * This task continuously reads audio data from I2S and performs
 * wake word detection using the wakenet model.
 */
static void wake_word_detect_task(void *arg)
{
    int audio_chunksize = s_wakenet_iface->get_samp_chunksize(s_model_data);
    int channel_num = s_wakenet_iface->get_channel_num(s_model_data);
    int sample_rate = s_wakenet_iface->get_samp_rate(s_model_data);

    ESP_LOGI(TAG, "Wake word detection started");
    ESP_LOGI(TAG, "Model config: chunksize=%d, model_channels=%d, sample_rate=%d",
             audio_chunksize, channel_num, sample_rate);
    ESP_LOGI(TAG, "I2S capture: %d ch interleaved, using channel %d",
             WAKE_WORD_I2S_CHANNELS, WAKE_WORD_CAPTURE_CHANNEL);
    ESP_LOGI(TAG, "Wake word: %s", s_wake_word_name);

    // Interleaved I2S frame buffer (all capture channels)
    const size_t frame_samples = (size_t)audio_chunksize * WAKE_WORD_I2S_CHANNELS;
    const size_t read_size = frame_samples * sizeof(int16_t);
    int16_t *i2s_buffer = malloc(read_size);
    // Mono buffer fed to the wakenet model
    int16_t *mono_buffer = malloc(audio_chunksize * sizeof(int16_t));
    if (i2s_buffer == NULL || mono_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers");
        free(i2s_buffer);
        free(mono_buffer);
        s_task_running = false;
        vTaskDelete(NULL);
        return;
    }

    size_t bytes_read;
    wakenet_state_t detect_result;

    while (s_task_running) {
        // Read one interleaved audio frame from I2S
        esp_err_t ret = i2s_read(I2S_NUM_0, i2s_buffer,
                                  read_size,
                                  &bytes_read, pdMS_TO_TICKS(100));

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(ret));
            continue;
        }

        if (bytes_read < read_size) {
            continue;
        }

        // De-interleave: extract a single capture channel into the mono buffer
        for (int i = 0; i < audio_chunksize; i++) {
            mono_buffer[i] = i2s_buffer[i * WAKE_WORD_I2S_CHANNELS + WAKE_WORD_CAPTURE_CHANNEL];
        }

        // Forward the captured mono stream (e.g. for cloud uplink). Kept off the
        // detection result so audio flows continuously; the consumer decides when
        // to actually stream.
        if (s_pcm_callback != NULL) {
            s_pcm_callback(mono_buffer, audio_chunksize);
        }

        // Perform wake word detection on the mono stream
        detect_result = s_wakenet_iface->detect(s_model_data, mono_buffer);

        if (detect_result == WAKENET_DETECTED) {
            int wake_word_index = s_wakenet_iface->get_triggered_channel(s_model_data);
            ESP_LOGI(TAG, "Wake word detected! Index: %d", wake_word_index);

            // Call callback if registered
            if (s_wake_word_callback != NULL) {
                s_wake_word_callback(wake_word_index, s_wake_word_name);
            }

            // Note: WakeNet keeps detecting after a trigger; do NOT call
            // s_wakenet_iface->clean() here. On this WakeNet9 model clean()
            // faults (LoadProhibited), and the reference esp-sr flow never
            // calls it. The delay below debounces immediate re-triggers.
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    // Cleanup
    free(i2s_buffer);
    free(mono_buffer);
    ESP_LOGI(TAG, "Wake word detection stopped");
    vTaskDelete(NULL);
}

esp_err_t wake_word_task_start(wake_word_cb_t callback)
{
    if (s_task_running) {
        ESP_LOGW(TAG, "Wake word task already running");
        return ESP_OK;
    }

    // Load all models packed into the "model" SPIFFS partition
    s_models = esp_srmodel_init(WAKE_WORD_MODEL_PARTITION);
    if (s_models == NULL || s_models->num <= 0) {
        ESP_LOGE(TAG, "No models found in partition '%s'", WAKE_WORD_MODEL_PARTITION);
        return ESP_ERR_NOT_FOUND;
    }

    // Find the NIHAODONGDONG wakenet model (你好东东)
    char *model_name = esp_srmodel_filter(s_models, ESP_WN_PREFIX, WAKE_WORD_MODEL_NAME);
    if (model_name == NULL) {
        ESP_LOGE(TAG, "Wake word model '%s' not found. Enable SR_WN_WN9_NIHAODONGDONG_TTS2 in menuconfig",
                 WAKE_WORD_MODEL_NAME);
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    // Get wakenet model handle
    s_wakenet_iface = esp_wn_handle_from_name(model_name);
    if (s_wakenet_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get wakenet handle for model: %s", model_name);
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    // Create wakenet model instance
    s_model_data = s_wakenet_iface->create(model_name, DET_MODE_90);
    if (s_model_data == NULL) {
        ESP_LOGE(TAG, "Failed to create wakenet model");
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Get wake word name
    s_wake_word_name = esp_srmodel_get_wake_words(s_models, model_name);

    // Save callback
    s_wake_word_callback = callback;

    // Start detection task
    s_task_running = true;
    BaseType_t ret = xTaskCreatePinnedToCore(
        wake_word_detect_task,
        "wake_word",
        WAKE_WORD_TASK_STACK_SIZE,
        NULL,
        WAKE_WORD_TASK_PRIORITY,
        &s_wake_word_task_handle,
        WAKE_WORD_TASK_CORE_ID
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wake word task");
        s_task_running = false;
        s_wakenet_iface->destroy(s_model_data);
        s_model_data = NULL;
        esp_srmodel_deinit(s_models);
        s_models = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t wake_word_task_stop(void)
{
    if (!s_task_running) {
        return ESP_OK;
    }
    
    // Signal task to stop
    s_task_running = false;
    
    // Wait for task to finish
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Cleanup model
    if (s_wakenet_iface != NULL && s_model_data != NULL) {
        s_wakenet_iface->destroy(s_model_data);
        s_model_data = NULL;
    }

    // Release loaded model list
    if (s_models != NULL) {
        esp_srmodel_deinit(s_models);
        s_models = NULL;
    }

    s_wake_word_callback = NULL;
    s_wake_word_task_handle = NULL;

    return ESP_OK;
}

void wake_word_task_set_pcm_callback(wake_word_pcm_cb_t cb)
{
    s_pcm_callback = cb;
}