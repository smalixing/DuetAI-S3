/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
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
#include "wifi_sta.h"
#include "joyinside.h"
#include "joyinside_opus.h"

#include "hal_log.h"

static const char *TAG = "main";

// Voice prompt played on wake word detection
#define WAKE_PROMPT_WAV     "/voice/I_comeon.wav"

// JoyInside audio format (16 kHz mono, 60 ms opus frame)
#define JI_SAMPLE_RATE      (16000)

// JoyInside client handle; NULL when the cloud session is unavailable
static joyinside_handle_t s_joyinside = NULL;

// Set while microphone audio should be streamed to the cloud for the current
// turn. Written by the RX task (event_cb) and the wake callback, read by the
// wake word task's PCM callback. A single-byte flag needs no lock.
static volatile bool s_streaming = false;

// True once TTS PCM playback has been started for the current response, so the
// codec is powered/unmuted lazily on the first decoded frame. Touched only from
// the JoyInside RX task (pcm_cb / event_cb run on the same thread).
static bool s_playing = false;

// Uplink accumulation buffer: the wake word task delivers audio in wakenet-sized
// chunks (typically 512 samples), but JoyInside expects exactly one 960-sample
// (60 ms) frame per send. Accumulate until a full frame is ready.
static int16_t s_up_buf[JOYINSIDE_OPUS_FRAME_SAMPLES];
static int s_up_len = 0;

/**
 * @brief Forward captured mic PCM to the cloud, one 60 ms frame at a time.
 *
 * Runs on the wake word task. Only streams while s_streaming is set.
 */
static void uplink_pcm_callback(const int16_t *pcm, int samples)
{
    if (!s_streaming || s_joyinside == NULL) {
        s_up_len = 0;
        return;
    }

    for (int i = 0; i < samples; i++) {
        s_up_buf[s_up_len++] = pcm[i];
        if (s_up_len == JOYINSIDE_OPUS_FRAME_SAMPLES) {
            joyinside_send_audio(s_joyinside, s_up_buf, s_up_len);
            s_up_len = 0;
        }
    }
}

/**
 * @brief Decoded TTS PCM from the cloud; play it over the codec DAC.
 *
 * Runs on the JoyInside RX task.
 */
static void joyinside_pcm_callback(const int16_t *pcm, int samples, void *user_ctx)
{
    (void)user_ctx;
    if (!s_playing) {
        audio_player_pcm_begin(JI_SAMPLE_RATE);
        s_playing = true;
    }
    audio_player_pcm_write(pcm, samples);
}

/**
 * @brief JoyInside lifecycle / turn events.
 *
 * Runs on the JoyInside RX task. Drives the per-turn streaming/playback state.
 */
static void joyinside_event_callback(joyinside_event_t event, void *user_ctx)
{
    (void)user_ctx;
    switch (event) {
        case JOYINSIDE_EVENT_ASR_FINAL:
            // Server has the full utterance; stop uplink for this turn.
            s_streaming = false;
            break;
        case JOYINSIDE_EVENT_INTERRUPTED:
        case JOYINSIDE_EVENT_TTS_COMPLETE:
        case JOYINSIDE_EVENT_CHAT_EXIT:
            // Response finished (or was cut short); tear down playback and
            // return to the idle/wait-for-wake state.
            s_streaming = false;
            if (s_playing) {
                audio_player_pcm_end();
                s_playing = false;
            }
            break;
        default:
            break;
    }
}

/**
 * @brief ASR / TTS text; logged for visibility.
 *
 * Runs on the JoyInside RX task.
 */
static void joyinside_text_callback(bool is_asr, const char *text, void *user_ctx)
{
    (void)user_ctx;
    hal_log_info("%s: %s", is_asr ? "ASR" : "TTS", text);
}

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

    // Open a voice turn: start uplink so the following speech reaches the cloud.
    if (s_joyinside != NULL && joyinside_is_connected(s_joyinside)) {
        s_up_len = 0;
        if (joyinside_chat_update(s_joyinside) == JOYINSIDE_ERR_OK) {
            s_streaming = true;
        } else {
            hal_log_warn("Wake: chat_update failed, no uplink this turn");
        }
    }
}

/**
 * @brief Connect to the JoyInside cloud and register audio/text/event callbacks.
 *
 * Credential fields are left NULL so the client falls back to its Kconfig
 * defaults. On any failure the cloud session stays disabled; local wake-word
 * detection still works.
 */
static void joyinside_session_start(void)
{
    joyinside_config_t cfg = {
        .version = JOYINSIDE_CONFIG_VERSION,
        .event_cb = joyinside_event_callback,
        .pcm_cb = joyinside_pcm_callback,
        .text_cb = joyinside_text_callback,
        .user_ctx = NULL,
    };

    s_joyinside = joyinside_create(&cfg);
    if (s_joyinside == NULL) {
        hal_log_err("JoyInside create failed");
        return;
    }

    if (joyinside_connect(s_joyinside) != JOYINSIDE_ERR_OK) {
        hal_log_err("JoyInside connect failed");
        joyinside_destroy(s_joyinside);
        s_joyinside = NULL;
        return;
    }

    hal_log_info("JoyInside session ready");
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

    /* Bring up WiFi and open the JoyInside cloud session (best-effort) */
    ret = wifi_sta_start();
    if (ESP_OK != ret) {
        hal_log_err("WiFi start failed: %s", esp_err_to_name(ret));
        // Continue; local wake word detection works without the cloud
    } else {
        joyinside_session_start();
    }

    /* Start wake word detection */
    hal_log_info("Starting wake word detection...");
    wake_word_task_set_pcm_callback(uplink_pcm_callback);
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
