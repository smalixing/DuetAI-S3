/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "esp_joyinside_chat.h"
#include "spiffs_audio.h"

static const char *TAG = "spiffs_audio";

esp_err_t spiffs_audio_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 4,
        .format_if_mount_failed = false,
    };
    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&conf), TAG, "mount SPIFFS failed");

    size_t total = 0;
    size_t used = 0;
    ESP_RETURN_ON_ERROR(esp_spiffs_info(conf.partition_label, &total, &used),
                        TAG, "read SPIFFS info failed");
    ESP_LOGI(TAG, "SPIFFS mounted: total=%u used=%u", (unsigned)total, (unsigned)used);
    return ESP_OK;
}

void spiffs_audio_unmount(void)
{
    esp_vfs_spiffs_unregister("storage");
}

esp_err_t spiffs_audio_send_pcm_file(esp_joyinside_chat_handle_t chat,
                                     const char *path,
                                     size_t frame_bytes,
                                     int frame_interval_ms)
{
    ESP_RETURN_ON_FALSE(chat != NULL && path != NULL && frame_bytes > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid PCM send args");

    FILE *file = fopen(path, "rb");
    ESP_RETURN_ON_FALSE(file != NULL, ESP_ERR_NOT_FOUND, TAG, "open %s failed", path);

    struct stat st = {0};
    if (stat(path, &st) != 0 || st.st_size <= 0) {
        fclose(file);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *frame = malloc(frame_bytes);
    if (frame == NULL) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "streaming %s (%ld bytes, frame=%u)", path, (long)st.st_size,
             (unsigned)frame_bytes);

    size_t sent_frames = 0;
    size_t sent_bytes = 0;
    while (true) {
        size_t read_len = fread(frame, 1, frame_bytes, file);
        if (read_len == 0) {
            break;
        }

        esp_err_t err = esp_joyinside_chat_send_audio_data(chat, frame, read_len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "send frame %u failed: %s", (unsigned)sent_frames,
                     esp_err_to_name(err));
            free(frame);
            fclose(file);
            return err;
        }

        sent_frames++;
        sent_bytes += read_len;
        if (frame_interval_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(frame_interval_ms));
        }
    }

    free(frame);
    fclose(file);
    ESP_LOGI(TAG, "sent %u PCM frames (%u bytes) from SPIFFS", (unsigned)sent_frames,
             (unsigned)sent_bytes);
    return ESP_OK;
}
