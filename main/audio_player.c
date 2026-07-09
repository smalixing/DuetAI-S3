/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/i2s.h"

#include "audio_player.h"
#include "bsp_board.h"
#include "bsp_codec.h"

static const char *TAG = "audio_player";

#define VOICE_PARTITION_LABEL   "voice"
#define VOICE_BASE_PATH         "/voice"

// Samples read from file per iteration (mono). Stereo output is 2x this.
#define PCM_CHUNK_SAMPLES       (512)

static bool s_initialized = false;

// Minimal RIFF/WAVE header fields we care about
typedef struct {
    uint16_t audio_format;   // 1 = PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_offset;    // byte offset of PCM samples in file
    uint32_t data_size;      // size of PCM data in bytes
} wav_info_t;

static uint32_t read_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Parse a RIFF/WAVE header, scanning chunks to locate "fmt " and "data".
 */
static esp_err_t wav_parse(FILE *fp, wav_info_t *info)
{
    uint8_t hdr[12];
    if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        return ESP_FAIL;
    }
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Not a RIFF/WAVE file");
        return ESP_FAIL;
    }

    bool have_fmt = false;
    bool have_data = false;
    uint8_t chunk[8];

    while (fread(chunk, 1, sizeof(chunk), fp) == sizeof(chunk)) {
        uint32_t chunk_size = read_u32le(chunk + 4);

        if (memcmp(chunk, "fmt ", 4) == 0) {
            uint8_t fmt[16];
            uint32_t to_read = chunk_size < sizeof(fmt) ? chunk_size : sizeof(fmt);
            if (fread(fmt, 1, to_read, fp) != to_read) {
                return ESP_FAIL;
            }
            info->audio_format   = read_u16le(fmt + 0);
            info->num_channels   = read_u16le(fmt + 2);
            info->sample_rate    = read_u32le(fmt + 4);
            info->bits_per_sample = read_u16le(fmt + 14);
            have_fmt = true;
            // Skip any remaining fmt bytes (chunks are word-aligned)
            long skip = (long)chunk_size - (long)to_read;
            if (chunk_size & 1) skip += 1;
            if (skip > 0) fseek(fp, skip, SEEK_CUR);
        } else if (memcmp(chunk, "data", 4) == 0) {
            info->data_offset = (uint32_t)ftell(fp);
            info->data_size = chunk_size;
            have_data = true;
            break;
        } else {
            // Skip unknown chunk (word-aligned)
            long skip = chunk_size + (chunk_size & 1);
            fseek(fp, skip, SEEK_CUR);
        }
    }

    if (!have_fmt || !have_data) {
        ESP_LOGE(TAG, "Missing fmt/data chunk");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t audio_player_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = VOICE_BASE_PATH,
        .partition_label = VOICE_PARTITION_LABEL,
        .max_files = 4,
        .format_if_mount_failed = false,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount voice SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    if (esp_spiffs_info(VOICE_PARTITION_LABEL, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "Voice SPIFFS mounted: %d/%d bytes used", (int)used, (int)total);
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t audio_player_play_file(const char *path)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "audio_player not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Cannot open %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    wav_info_t info = {0};
    if (wav_parse(fp, &info) != ESP_OK) {
        fclose(fp);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Playing %s: %u Hz, %u ch, %u-bit, %u bytes",
             path, (unsigned)info.sample_rate, (unsigned)info.num_channels,
             (unsigned)info.bits_per_sample, (unsigned)info.data_size);

    if (info.audio_format != 1 || info.bits_per_sample != 16 || info.num_channels != 1) {
        ESP_LOGE(TAG, "Unsupported WAV format (need PCM 16-bit mono)");
        fclose(fp);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Match the codec sample rate to the file
    i2s_set_sample_rates(I2S_NUM_0, info.sample_rate);

    // Enable power amplifier and unmute the DAC
    bsp_board_power_ctrl(POWER_MODULE_AUDIO, true);
    bsp_codec_set_mute(false);

    int16_t *mono = malloc(PCM_CHUNK_SAMPLES * sizeof(int16_t));
    // Stereo output: duplicate each mono sample to L/R
    int16_t *stereo = malloc(PCM_CHUNK_SAMPLES * 2 * sizeof(int16_t));
    if (mono == NULL || stereo == NULL) {
        ESP_LOGE(TAG, "Failed to allocate playback buffers");
        free(mono);
        free(stereo);
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    fseek(fp, info.data_offset, SEEK_SET);
    uint32_t remaining = info.data_size;
    size_t bytes_written;

    while (remaining > 0) {
        size_t want = PCM_CHUNK_SAMPLES * sizeof(int16_t);
        if (remaining < want) {
            want = remaining;
        }
        size_t got = fread(mono, 1, want, fp);
        if (got == 0) {
            break;
        }
        int samples = got / sizeof(int16_t);
        for (int i = 0; i < samples; i++) {
            stereo[i * 2]     = mono[i];
            stereo[i * 2 + 1] = mono[i];
        }
        i2s_write(I2S_NUM_0, stereo, samples * 2 * sizeof(int16_t),
                  &bytes_written, portMAX_DELAY);
        remaining -= got;
    }

    // Flush and mute to avoid idle hiss
    i2s_zero_dma_buffer(I2S_NUM_0);
    bsp_codec_set_mute(true);

    free(mono);
    free(stereo);
    fclose(fp);

    ESP_LOGI(TAG, "Playback done: %s", path);
    return ESP_OK;
}

esp_err_t audio_player_pcm_begin(uint32_t sample_rate)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "PCM begin failed: audio_player not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2s_set_sample_rates(I2S_NUM_0, sample_rate);
    bsp_board_power_ctrl(POWER_MODULE_AUDIO, true);
    bsp_codec_set_mute(false);
    return ESP_OK;
}

esp_err_t audio_player_pcm_write(const int16_t *pcm, int samples)
{
    if (pcm == NULL || samples <= 0) {
        ESP_LOGE(TAG, "PCM write failed: pcm is NULL or samples <= 0");
        return ESP_ERR_INVALID_ARG;
    }

    // Stereo output: duplicate each mono sample to L/R
    int16_t *stereo = malloc((size_t)samples * 2 * sizeof(int16_t));
    if (stereo == NULL) {
        ESP_LOGE(TAG, "PCM write failed: OOM stereo buffer");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < samples; i++) {
        stereo[i * 2]     = pcm[i];
        stereo[i * 2 + 1] = pcm[i];
    }

    size_t bytes_written;
    i2s_write(I2S_NUM_0, stereo, (size_t)samples * 2 * sizeof(int16_t),
              &bytes_written, portMAX_DELAY);

    free(stereo);
    return ESP_OK;
}

void audio_player_pcm_end(void)
{
    i2s_zero_dma_buffer(I2S_NUM_0);
    bsp_codec_set_mute(true);
}
