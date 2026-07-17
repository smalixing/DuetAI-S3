/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"

#include "mbedtls/base64.h"

#include "cJSON.h"

#include "hal_websocket.h"
#include "hal_log.h"
#include "os_hal.h"

#include "joyinside.h"
#include "joyinside_internal.h"
#include "joyinside_auth.h"
#include "joyinside_protocol.h"
#include "joyinside_opus.h"

#define CONNECT_TIMEOUT_MS      (5 * 1000)
/* RX read timeout. A silent link returns HAL_WS_CLOSED at this bound, so it also
 * caps how long joyinside_disconnect waits for the RX task to observe shutdown. */
#define WS_READ_TIMEOUT_MS      (15 * 1000)
#define WS_WRITE_TIMEOUT_MS     (6 * 1000)
/* Upper bound for disconnect to wait for the RX/TX tasks to exit (> read timeout). */
#define TASK_EXIT_WAIT_MS       (WS_READ_TIMEOUT_MS + 3000)
/* Keepalive: the server closes an idle connection after ~15 s with no traffic.
 * The TX task sends a WS ping this often when there is nothing to uplink, so the
 * link stays open between voice turns. Must be well under the server's idle
 * window (the reference client pings every 10 s). */
#define WS_PING_INTERVAL_MS     (5 * 1000)

/* Task stacks come from PSRAM (see hal_thread_create). esp-opus is built with
 * -DUSE_ALLOCA, so the encoder/decoder scratch lives on these stacks and the
 * fixed-point SILK encoder needs a large chunk even at complexity 0. Measured
 * TX peak use is ~24 KB (opus_encode); 32 KB leaves headroom. RX runs the
 * lighter opus_decode. */
#define RX_TASK_STACK           (16 * 1024)
#define TX_TASK_STACK           (32 * 1024)
#define RX_TASK_PRIO            (5)
#define TX_TASK_PRIO            (5)

#define TEXT_REASSEMBLY_MAX     (16 * 1024)
#define OUT_JSON_MAX            (4 * 1024)
#define OPUS_PACKET_MAX         (1024)
#define JWT_MAX                 (256)

#define UPLINK_QUEUE_LEN        (8)

/* Event group bits shared by the RX/TX tasks */
#define EVT_STOP                (0x01 << 0)
#define EVT_AUDIO_SEND          (0x01 << 1)

#define CFG_STR(field, kconfig_default) \
    ((field) && (field)[0] ? (field) : (kconfig_default))

/**
 * @brief  One queued uplink item.
 *
 *         Normally a 16-bit mono PCM frame (samples > 0). When finish is set the
 *         item carries no audio and marks the end of the turn's uplink; the TX
 *         task sends CLIENT_AUDIO_FINISH after the preceding audio frames, so
 *         the marker never overtakes queued-but-unsent audio.
 */
typedef struct {
    int16_t pcm[JOYINSIDE_OPUS_FRAME_SAMPLES];
    int     samples;
    bool    finish;
} uplink_frame_t;

/**
 * @brief  JoyInside client context (opaque to callers)
 */
struct joyinside {
    char                 uri[128];       /*!< Base endpoint URI */
    char                 access_key_id[64];
    char                 access_key_secret[64];
    char                 bot_id[64];
    char                 device_token[128];
    char                 device_id[64];

    joyinside_event_cb_t event_cb;       /*!< Status event callback */
    joyinside_pcm_cb_t   pcm_cb;        /*!< Decoded TTS PCM callback */
    joyinside_text_cb_t  text_cb;       /*!< ASR / TTS text callback */
    void                *user_ctx;       /*!< Opaque user pointer */

    void                *ws;             /*!< hal_ws connection context */
    void                *ws_mux;         /*!< Serializes outbound writes */
    void                *event;          /*!< Event group for task control */
    void                *uplink_queue;   /*!< uplink_frame_t queue */

    void                *rx_task;
    void                *tx_task;

    joyinside_opus_handle_t codec;

    char                 mid[JOYINSIDE_UUID_STR_LEN + 1];  /*!< Session message id */

    uint8_t             *text_buf;       /*!< TEXT frame reassembly buffer */
    int                  text_len;

    int16_t             *decode_pcm;     /*!< Scratch PCM buffer for decode */

    volatile bool        running;
    volatile bool        connected;
};

/* ---------------- internal write helper ---------------- */

static joyinside_err_t _ws_send_text(struct joyinside *ji, const char *text, int len)
{
    if (!ji->connected) {
        hal_log_warn("Send text: not connected, drop");
        return JOYINSIDE_ERR_FAIL;
    }

    hal_mutex_lock(ji->ws_mux);
    int ret = hal_ws_write_text(ji->ws, (const uint8_t *)text, len, WS_WRITE_TIMEOUT_MS);
    hal_mutex_unlock(ji->ws_mux);

    if (ret != 0) {
        hal_log_err("Send text failed: hal_ws_write_text ret=%d", ret);
        return JOYINSIDE_ERR_FAIL;
    }
    return JOYINSIDE_ERR_OK;
}

/* ---------------- internal callback hooks (joyinside_internal.h) ---------------- */

void ji_emit_event(joyinside_handle_t handle, joyinside_event_t event)
{
    if (handle && handle->event_cb) {
        handle->event_cb(event, handle->user_ctx);
    }
}

void ji_emit_text(joyinside_handle_t handle, bool is_asr, const char *text)
{
    if (handle && handle->text_cb) {
        handle->text_cb(is_asr, text, handle->user_ctx);
    }
}

void ji_emit_tts_base64(joyinside_handle_t handle, const char *audio_base64)
{
    if (!handle || !handle->pcm_cb) {
        return;
    }

    uint8_t opus_buf[OPUS_PACKET_MAX];
    size_t olen = 0;
    int ret = mbedtls_base64_decode(opus_buf, sizeof(opus_buf), &olen,
                                    (const unsigned char *)audio_base64, strlen(audio_base64));
    if (ret != 0) {
        hal_log_warn("TTS base64 decode failed: ret=%d", ret);
        return;
    }

    int samples = joyinside_opus_decode(handle->codec, opus_buf, (int)olen,
                                        handle->decode_pcm, JOYINSIDE_OPUS_FRAME_SAMPLES * 2);
    if (samples > 0) {
        handle->pcm_cb(handle->decode_pcm, samples, handle->user_ctx);
    }
}

/* ---------------- RX task ---------------- */

static void _rx_consume_text(struct joyinside *ji, const uint8_t *data, int len)
{
    if (ji->text_len + len >= TEXT_REASSEMBLY_MAX) {
        hal_log_warn("RX text buffer overflow, reset");
        ji->text_len = 0;
        return;
    }
    memcpy(ji->text_buf + ji->text_len, data, len);
    ji->text_len += len;
    ji->text_buf[ji->text_len] = '\0';

    /* One TCP read may carry several concatenated JSON objects. */
    const char *parse_end = NULL;
    while (ji->text_len > 0) {
        cJSON *root = cJSON_ParseWithOpts((const char *)ji->text_buf, &parse_end, 0);
        if (root == NULL) {
            break;
        }
        joyinside_protocol_dispatch(ji, root);
        cJSON_Delete(root);

        int consumed = (int)((const uint8_t *)parse_end - ji->text_buf);
        ji->text_len -= consumed;
        if (ji->text_len > 0) {
            memmove(ji->text_buf, parse_end, ji->text_len);
            ji->text_buf[ji->text_len] = '\0';
        }
    }
}

static void _rx_task(void *arg)
{
    struct joyinside *ji = (struct joyinside *)arg;
    /* 16 KB scratch: keep it out of scarce internal RAM. */
    uint8_t *frame = (uint8_t *)heap_caps_malloc(TEXT_REASSEMBLY_MAX, MALLOC_CAP_SPIRAM);
    if (frame == NULL) {
        hal_log_err("RX task failed: OOM frame buffer");
        ji->running = false;
        hal_thread_delete(NULL);
        return;
    }

    while (ji->running) {
        int opcode = 0;
        int n = hal_ws_read_ex(ji->ws, frame, TEXT_REASSEMBLY_MAX - 1, &opcode, WS_READ_TIMEOUT_MS);

        if (n == HAL_WS_CONTROL) {
            continue;  /* PING/PONG handled inside the HAL */
        }
        if (n == HAL_WS_CLOSED) {
            hal_log_info("RX: peer closed connection");
            break;
        }
        if (n == HAL_WS_ERR) {
            hal_log_err("RX failed: read error");
            break;
        }
        if (n == 0) {
            continue;  /* read timeout, keep waiting */
        }

        if (opcode == HAL_WS_OPCODE_BINARY) {
            int samples = joyinside_opus_decode(ji->codec, frame, n,
                                                ji->decode_pcm, JOYINSIDE_OPUS_FRAME_SAMPLES * 2);
            if (samples > 0 && ji->pcm_cb) {
                ji->pcm_cb(ji->decode_pcm, samples, ji->user_ctx);
            }
        } else {
            _rx_consume_text(ji, frame, n);
        }
    }

    free(frame);
    ji->connected = false;
    ji_emit_event(ji, JOYINSIDE_EVENT_DISCONNECTED);
    hal_log_info("RX task stopped");
    ji->rx_task = NULL;
    hal_thread_delete(NULL);
}

/* ---------------- TX task ---------------- */

static void _tx_task(void *arg)
{
    struct joyinside *ji = (struct joyinside *)arg;
    uint8_t *opus_buf = (uint8_t *)heap_caps_malloc(OPUS_PACKET_MAX, MALLOC_CAP_SPIRAM);
    uplink_frame_t *frame = (uplink_frame_t *)heap_caps_malloc(sizeof(uplink_frame_t), MALLOC_CAP_SPIRAM);
    if (opus_buf == NULL || frame == NULL) {
        hal_log_err("TX task failed: OOM");
        free(opus_buf);
        free(frame);
        ji->tx_task = NULL;
        hal_thread_delete(NULL);
        return;
    }

    while (ji->running) {
        uint32_t bits = hal_event_wait(ji->event, EVT_STOP | EVT_AUDIO_SEND, 0, 0, WS_PING_INTERVAL_MS);
        if ((bits & EVT_STOP) || !ji->running) {
            break;
        }
        if (!(bits & EVT_AUDIO_SEND)) {
            /* Idle wait timed out: keep the link alive so the server does not
             * close it between voice turns. */
            if (ji->connected) {
                hal_mutex_lock(ji->ws_mux);
                hal_ws_send_ping_frame(ji->ws, WS_WRITE_TIMEOUT_MS);
                hal_mutex_unlock(ji->ws_mux);
            }
            continue;
        }

        /* Drain queued frames while the send bit is set and we stay connected. */
        while (ji->running && ji->connected &&
               hal_queue_recv(ji->uplink_queue, frame, 0) == 0) {
            if (frame->finish) {
                /* End-of-turn marker: all preceding audio frames have been sent
                 * above, so tell the server uplink is complete for this turn. */
                char json[128];
                int jlen = joyinside_protocol_build_audio_finish(json, sizeof(json), ji->mid);
                if (jlen > 0) {
                    _ws_send_text(ji, json, jlen);
                }
                continue;
            }
            int len = joyinside_opus_encode(ji->codec, frame->pcm, frame->samples,
                                            opus_buf, OPUS_PACKET_MAX);
            if (len <= 0) {
                continue;
            }
            hal_mutex_lock(ji->ws_mux);
            int ret = hal_ws_write(ji->ws, opus_buf, len, WS_WRITE_TIMEOUT_MS);
            hal_mutex_unlock(ji->ws_mux);
            if (ret != 0) {
                hal_log_warn("TX: opus frame send failed, drop");
            }
        }
        hal_event_clear(ji->event, EVT_AUDIO_SEND);
    }

    free(opus_buf);
    free(frame);
    hal_log_info("TX task stopped");
    ji->tx_task = NULL;
    hal_thread_delete(NULL);
}

/* ---------------- lifecycle ---------------- */

joyinside_handle_t joyinside_create(const joyinside_config_t *config)
{
    if (config == NULL || config->version != JOYINSIDE_CONFIG_VERSION) {
        hal_log_err("Create failed: NULL or version mismatch");
        return NULL;
    }

    struct joyinside *ji = (struct joyinside *)heap_caps_calloc(1, sizeof(struct joyinside), MALLOC_CAP_SPIRAM);
    if (ji == NULL) {
        hal_log_err("Create failed: OOM");
        return NULL;
    }

    snprintf(ji->uri, sizeof(ji->uri), "%s", CFG_STR(config->uri, CONFIG_JOYINSIDE_URI));
    snprintf(ji->access_key_id, sizeof(ji->access_key_id), "%s",
             CFG_STR(config->access_key_id, CONFIG_JOYINSIDE_ACCESS_KEY_ID));
    snprintf(ji->access_key_secret, sizeof(ji->access_key_secret), "%s",
             CFG_STR(config->access_key_secret, CONFIG_JOYINSIDE_ACCESS_KEY_SECRET));
    snprintf(ji->bot_id, sizeof(ji->bot_id), "%s", CFG_STR(config->bot_id, CONFIG_JOYINSIDE_BOT_ID));
    snprintf(ji->device_token, sizeof(ji->device_token), "%s",
             CFG_STR(config->device_token, CONFIG_JOYINSIDE_DEVICE_TOKEN));
    snprintf(ji->device_id, sizeof(ji->device_id), "%s",
             CFG_STR(config->device_id, CONFIG_JOYINSIDE_DEVICE_ID));

    ji->event_cb = config->event_cb;
    ji->pcm_cb = config->pcm_cb;
    ji->text_cb = config->text_cb;
    ji->user_ctx = config->user_ctx;

    ji->text_buf = (uint8_t *)heap_caps_malloc(TEXT_REASSEMBLY_MAX, MALLOC_CAP_SPIRAM);
    ji->decode_pcm = (int16_t *)heap_caps_malloc(JOYINSIDE_OPUS_FRAME_SAMPLES * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    ji->ws_mux = hal_mutex_create("ji_ws");
    ji->event = hal_event_create("ji_evt");
    ji->uplink_queue = hal_queue_create("ji_up", sizeof(uplink_frame_t), UPLINK_QUEUE_LEN);
    ji->codec = joyinside_opus_create();

    if (!ji->text_buf || !ji->decode_pcm || !ji->ws_mux || !ji->event ||
        !ji->uplink_queue || !ji->codec) {
        hal_log_err("Create failed: resource allocation error");
        joyinside_destroy(ji);
        return NULL;
    }

    hal_log_info("Create 'joyinside': client ready");
    return ji;
}

void joyinside_destroy(joyinside_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    joyinside_disconnect(handle);

    if (handle->codec) {
        joyinside_opus_destroy(handle->codec);
    }
    if (handle->uplink_queue) {
        hal_queue_destroy(handle->uplink_queue);
    }
    if (handle->event) {
        hal_event_destroy(handle->event);
    }
    if (handle->ws_mux) {
        hal_mutex_destroy(handle->ws_mux);
    }
    free(handle->text_buf);
    free(handle->decode_pcm);
    free(handle);
    hal_log_info("Destroy 'joyinside': done");
}

joyinside_err_t joyinside_connect(joyinside_handle_t handle)
{
    if (handle == NULL) {
        hal_log_err("Connect failed: handle is NULL");
        return JOYINSIDE_ERR_NOT_INIT;
    }
    if (handle->connected) {
        hal_log_warn("Connect: already connected");
        return JOYINSIDE_ERR_BUSY;
    }

    char *uri = joyinside_auth_build_uri(handle->uri, handle->access_key_id,
                                         handle->access_key_secret, handle->bot_id);
    if (uri == NULL) {
        hal_log_err("Connect failed: URI build error");
        return JOYINSIDE_ERR_INVALID_ARG;
    }

    handle->ws = hal_ws_connect(uri, 0, NULL, CONNECT_TIMEOUT_MS);
    free(uri);
    if (handle->ws == NULL) {
        hal_log_err("Connect failed: websocket connect error");
        return JOYINSIDE_ERR_FAIL;
    }

    handle->text_len = 0;
    handle->running = true;
    handle->connected = true;
    hal_event_clear(handle->event, EVT_STOP | EVT_AUDIO_SEND);

    handle->rx_task = hal_thread_create("ji_rx", _rx_task, handle, RX_TASK_STACK, RX_TASK_PRIO);
    handle->tx_task = hal_thread_create("ji_tx", _tx_task, handle, TX_TASK_STACK, TX_TASK_PRIO);
    if (!handle->rx_task || !handle->tx_task) {
        hal_log_err("Connect failed: task create error");
        joyinside_disconnect(handle);
        return JOYINSIDE_ERR_FAIL;
    }

    ji_emit_event(handle, JOYINSIDE_EVENT_CONNECTED);
    hal_log_info("Connect 'joyinside': connected");
    return JOYINSIDE_ERR_OK;
}

joyinside_err_t joyinside_disconnect(joyinside_handle_t handle)
{
    if (handle == NULL) {
        return JOYINSIDE_ERR_NOT_INIT;
    }
    if (!handle->running && handle->ws == NULL) {
        return JOYINSIDE_ERR_OK;
    }

    handle->running = false;
    handle->connected = false;
    hal_event_set(handle->event, EVT_STOP);

    if (handle->ws) {
        hal_ws_send_close_frame(handle->ws, WS_WRITE_TIMEOUT_MS);
    }

    /* Wait for both tasks to fully exit before freeing ws: the RX task may be
     * blocked in hal_ws_read_ex for up to WS_READ_TIMEOUT_MS. Freeing ws while
     * it reads would be a use-after-free, so the tasks never touch ws teardown. */
    int waited = 0;
    while ((handle->rx_task || handle->tx_task) && waited < TASK_EXIT_WAIT_MS) {
        hal_thread_sleep(20);
        waited += 20;
    }
    if (handle->rx_task || handle->tx_task) {
        hal_log_warn("Disconnect: tasks did not exit in time");
    }

    if (handle->ws) {
        hal_ws_disconnect(handle->ws);
        handle->ws = NULL;
    }

    hal_log_info("Disconnect 'joyinside': done");
    return JOYINSIDE_ERR_OK;
}

int joyinside_is_connected(joyinside_handle_t handle)
{
    return (handle && handle->connected) ? 1 : 0;
}

joyinside_err_t joyinside_chat_update(joyinside_handle_t handle)
{
    if (handle == NULL) {
        return JOYINSIDE_ERR_NOT_INIT;
    }

    char jwt[JWT_MAX] = {0};
    if (joyinside_auth_build_jwt(handle->device_id, handle->device_token, jwt, sizeof(jwt)) != 0) {
        hal_log_err("Chat update failed: JWT build error");
        return JOYINSIDE_ERR_FAIL;
    }

    joyinside_auth_gen_uuid(handle->mid);

    char *json = (char *)malloc(OUT_JSON_MAX);
    if (json == NULL) {
        hal_log_err("Chat update failed: OOM");
        return JOYINSIDE_ERR_NO_MEM;
    }

    int len = joyinside_protocol_build_chat_update(json, OUT_JSON_MAX, handle->mid,
                                                   handle->device_id, handle->bot_id, jwt);
    joyinside_err_t ret = (len > 0) ? _ws_send_text(handle, json, len) : JOYINSIDE_ERR_FAIL;
    free(json);
    return ret;
}

joyinside_err_t joyinside_send_audio(joyinside_handle_t handle, const int16_t *pcm, int samples)
{
    if (handle == NULL || pcm == NULL) {
        return JOYINSIDE_ERR_INVALID_ARG;
    }
    if (samples != JOYINSIDE_OPUS_FRAME_SAMPLES) {
        hal_log_err("Send audio failed: expected %d samples, got %d",
                    JOYINSIDE_OPUS_FRAME_SAMPLES, samples);
        return JOYINSIDE_ERR_INVALID_ARG;
    }
    if (!handle->connected) {
        return JOYINSIDE_ERR_FAIL;
    }

    uplink_frame_t frame;
    memcpy(frame.pcm, pcm, samples * sizeof(int16_t));
    frame.samples = samples;
    frame.finish = false;

    if (hal_queue_send(handle->uplink_queue, &frame, 0) != 0) {
        hal_log_warn("Send audio: uplink queue full, frame dropped");
        return JOYINSIDE_ERR_FAIL;
    }
    hal_event_set(handle->event, EVT_AUDIO_SEND);
    return JOYINSIDE_ERR_OK;
}

joyinside_err_t joyinside_audio_finish(joyinside_handle_t handle)
{
    if (handle == NULL) {
        return JOYINSIDE_ERR_NOT_INIT;
    }
    if (!handle->connected) {
        return JOYINSIDE_ERR_FAIL;
    }

    /* Enqueue an end-of-turn marker so the TX task sends CLIENT_AUDIO_FINISH
     * only after every queued audio frame, preserving order. */
    uplink_frame_t frame = {0};
    frame.samples = 0;
    frame.finish = true;

    if (hal_queue_send(handle->uplink_queue, &frame, 0) != 0) {
        hal_log_warn("Audio finish: uplink queue full, marker dropped");
        return JOYINSIDE_ERR_FAIL;
    }
    hal_event_set(handle->event, EVT_AUDIO_SEND);
    return JOYINSIDE_ERR_OK;
}

joyinside_err_t joyinside_interrupt(joyinside_handle_t handle)
{
    if (handle == NULL) {
        return JOYINSIDE_ERR_NOT_INIT;
    }

    char json[256];
    int len = joyinside_protocol_build_interrupt(json, sizeof(json), handle->mid);
    return (len > 0) ? _ws_send_text(handle, json, len) : JOYINSIDE_ERR_FAIL;
}

joyinside_err_t joyinside_text_to_speech(joyinside_handle_t handle, const char *text)
{
    if (handle == NULL || text == NULL) {
        return JOYINSIDE_ERR_INVALID_ARG;
    }

    int cap = 256 + (int)strlen(text);
    char *json = (char *)malloc(cap);
    if (json == NULL) {
        hal_log_err("Text-to-speech failed: OOM");
        return JOYINSIDE_ERR_NO_MEM;
    }

    int len = joyinside_protocol_build_tts(json, cap, handle->mid, text);
    joyinside_err_t ret = (len > 0) ? _ws_send_text(handle, json, len) : JOYINSIDE_ERR_FAIL;
    free(json);
    return ret;
}

joyinside_err_t joyinside_text_input(joyinside_handle_t handle, const char *text)
{
    if (handle == NULL || text == NULL) {
        return JOYINSIDE_ERR_INVALID_ARG;
    }

    joyinside_auth_gen_uuid(handle->mid);

    int cap = 256 + (int)strlen(text);
    char *json = (char *)malloc(cap);
    if (json == NULL) {
        hal_log_err("Text-input failed: OOM");
        return JOYINSIDE_ERR_NO_MEM;
    }

    int len = joyinside_protocol_build_text_input(json, cap, handle->mid, text);
    joyinside_err_t ret = (len > 0) ? _ws_send_text(handle, json, len) : JOYINSIDE_ERR_FAIL;
    free(json);
    return ret;
}
