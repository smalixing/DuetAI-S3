/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_joyinside_chat.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t spiffs_audio_mount(void);
void spiffs_audio_unmount(void);
esp_err_t spiffs_audio_send_pcm_file(esp_joyinside_chat_handle_t chat,
                                     const char *path,
                                     size_t frame_bytes,
                                     int frame_interval_ms);

#ifdef __cplusplus
}
#endif
