/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "opus.h"

#include "hal_log.h"

#include "joyinside_opus.h"

#define OPUS_ENCODER_BITRATE    (24000)

struct joyinside_opus {
    OpusEncoder *encoder;
    OpusDecoder *decoder;
};

joyinside_opus_handle_t joyinside_opus_create(void)
{
    struct joyinside_opus *codec = (struct joyinside_opus *)calloc(1, sizeof(*codec));
    if (codec == NULL) {
        hal_log_err("Create 'opus' failed: OOM");
        return NULL;
    }

    int err = OPUS_OK;
    codec->encoder = opus_encoder_create(JOYINSIDE_OPUS_SAMPLE_RATE, JOYINSIDE_OPUS_CHANNELS,
                                         OPUS_APPLICATION_VOIP, &err);
    if (codec->encoder == NULL || err != OPUS_OK) {
        hal_log_err("Create 'opus' failed: encoder err=%d", err);
        goto fail;
    }
    opus_encoder_ctl(codec->encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
    opus_encoder_ctl(codec->encoder, OPUS_SET_VBR(0));

    codec->decoder = opus_decoder_create(JOYINSIDE_OPUS_SAMPLE_RATE, JOYINSIDE_OPUS_CHANNELS, &err);
    if (codec->decoder == NULL || err != OPUS_OK) {
        hal_log_err("Create 'opus' failed: decoder err=%d", err);
        goto fail;
    }

    hal_log_info("Create 'opus': 16kHz mono 60ms codec ready");
    return codec;

fail:
    joyinside_opus_destroy(codec);
    return NULL;
}

void joyinside_opus_destroy(joyinside_opus_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    if (handle->encoder) {
        opus_encoder_destroy(handle->encoder);
    }
    if (handle->decoder) {
        opus_decoder_destroy(handle->decoder);
    }
    free(handle);
}

int joyinside_opus_encode(joyinside_opus_handle_t handle, const int16_t *pcm, int samples,
                          uint8_t *out, int out_len)
{
    if (!handle || !pcm || !out) {
        hal_log_err("Opus encode failed: NULL argument");
        return -1;
    }

    int n = opus_encode(handle->encoder, pcm, samples, out, out_len);
    if (n < 0) {
        hal_log_err("Opus encode failed: err=%d", n);
        return -1;
    }
    return n;
}

int joyinside_opus_decode(joyinside_opus_handle_t handle, const uint8_t *data, int data_len,
                          int16_t *pcm, int max_samples)
{
    if (!handle || !data || !pcm) {
        hal_log_err("Opus decode failed: NULL argument");
        return -1;
    }

    int n = opus_decode(handle->decoder, data, data_len, pcm, max_samples, 0);
    if (n < 0) {
        hal_log_err("Opus decode failed: err=%d", n);
        return -1;
    }
    return n;
}
