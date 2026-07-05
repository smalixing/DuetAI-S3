/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_err.h"

#include "bsp_board.h"
#include "lvgl_port.h"
#include "wake_word_task.h"
#include "audio_player.h"

#include "hal_log.h"

static const char *TAG = "main";

// Voice prompt played on wake word detection
#define WAKE_PROMPT_WAV     "/voice/I_comeon.wav"

/**
 * @brief Wake word detection callback
 *
 * This function is called when a wake word is detected.
 *
 * @param wake_word_index The index of detected wake word
 * @param wake_word_name The name of detected wake word
 */
static void wake_word_detected_callback(int wake_word_index, const char *wake_word_name)
{
    hal_log_info("Wake word detected: %s (index: %d)", wake_word_name, wake_word_index);
    // Play the wake acknowledgement prompt. Runs on the wake word task, so
    // detection is naturally paused while the prompt plays (no self-trigger).
    audio_player_play_file(WAKE_PROMPT_WAV);
}

void app_main(void)
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    hal_log_info("%s chip with %d CPU core(s)", CONFIG_IDF_TARGET, chip_info.cores);
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        hal_log_info("%" PRIu32 "MB %s flash",
                 flash_size / (uint32_t)(1024 * 1024),
                 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    }
    hal_log_info("Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());

    /* Board and peripheral hardware initialization */
    hal_log_info("Initializing board...");
    esp_err_t ret = bsp_board_init();
    if (ESP_OK != ret) {
        hal_log_err("Board init failed: %s", esp_err_to_name(ret));
        return;
    }
    hal_log_info("Board init done");

    /* Initialize LVGL and light up the screen */
    ret = lvgl_port_init();
    if (ESP_OK != ret) {
        hal_log_err("LVGL port init failed: %s", esp_err_to_name(ret));
        return;
    }
    hal_log_info("LVGL port init done");

    /* Mount voice partition and init audio player (needs I2S + codec from board init) */
    ret = audio_player_init();
    if (ESP_OK != ret) {
        hal_log_err("Audio player init failed: %s", esp_err_to_name(ret));
        // Continue; wake word still works, just without the prompt
    } else {
        hal_log_info("Audio player init done");
    }

    /* Start wake word detection */
    hal_log_info("Starting wake word detection...");
    ret = wake_word_task_start(wake_word_detected_callback);
    if (ESP_OK != ret) {
        hal_log_err("Wake word task start failed: %s", esp_err_to_name(ret));
        // Continue even if wake word detection fails, as it's not critical
    } else {
        hal_log_info("Wake word detection started");
    }

    while (1) {
        printf("free heap size: %ld, internal size: %ld, minimum size: %ld\n",
            esp_get_free_heap_size(),
            esp_get_free_internal_heap_size(),
            esp_get_minimum_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
