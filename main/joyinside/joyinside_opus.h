/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JOYINSIDE_OPUS_H__
#define __JOYINSIDE_OPUS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Audio format used by the JoyInside voice-call protocol */
#define JOYINSIDE_OPUS_SAMPLE_RATE   (16000)
#define JOYINSIDE_OPUS_CHANNELS      (1)
#define JOYINSIDE_OPUS_FRAME_MS      (60)
/* Samples per channel in one 60 ms frame at 16 kHz */
#define JOYINSIDE_OPUS_FRAME_SAMPLES (JOYINSIDE_OPUS_SAMPLE_RATE * JOYINSIDE_OPUS_FRAME_MS / 1000)

typedef struct joyinside_opus *joyinside_opus_handle_t;

/**
 * @brief  Create an opus encoder+decoder pair for JoyInside audio
 *
 * @return  Handle on success, NULL on OOM / opus error
 */
joyinside_opus_handle_t joyinside_opus_create(void);

/**
 * @brief  Destroy the codec and free its buffers
 *
 * @param[in]  handle  Handle from joyinside_opus_create(); may be NULL
 */
void joyinside_opus_destroy(joyinside_opus_handle_t handle);

/**
 * @brief  Encode one PCM frame to an opus packet
 *
 * @param[in]   handle    Codec handle
 * @param[in]   pcm       Input PCM (16-bit, mono)
 * @param[in]   samples   Samples in pcm (must equal JOYINSIDE_OPUS_FRAME_SAMPLES)
 * @param[out]  out       Output buffer for the opus packet
 * @param[in]   out_len   Size of out
 *
 * @return  Encoded byte count (>0), or -1 on error
 */
int joyinside_opus_encode(joyinside_opus_handle_t handle, const int16_t *pcm, int samples,
                          uint8_t *out, int out_len);

/**
 * @brief  Decode one opus packet to a PCM frame
 *
 * @param[in]   handle      Codec handle
 * @param[in]   data        Opus packet bytes
 * @param[in]   data_len    Packet length
 * @param[out]  pcm         Output PCM buffer (16-bit, mono)
 * @param[in]   max_samples Capacity of pcm in samples
 *
 * @return  Decoded sample count (>0), or -1 on error
 */
int joyinside_opus_decode(joyinside_opus_handle_t handle, const uint8_t *data, int data_len,
                          int16_t *pcm, int max_samples);

#ifdef __cplusplus
}
#endif

#endif /* __JOYINSIDE_OPUS_H__ */
