/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JOYINSIDE_H__
#define __JOYINSIDE_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JOYINSIDE_CONFIG_VERSION  (1)

/**
 * @brief  Error codes for JoyInside cloud client operations
 */
typedef enum {
    JOYINSIDE_ERR_OK          = 0,   /*!< Operation successful */
    JOYINSIDE_ERR_FAIL        = -1,  /*!< Generic failure */
    JOYINSIDE_ERR_INVALID_ARG = -2,  /*!< NULL or out-of-range argument */
    JOYINSIDE_ERR_NO_MEM      = -3,  /*!< Allocation failed */
    JOYINSIDE_ERR_NOT_INIT    = -4,  /*!< Handle not initialized */
    JOYINSIDE_ERR_BUSY        = -5,  /*!< Already connected / in use */
    JOYINSIDE_ERR_TIMEOUT     = -6,  /*!< Operation timed out */
} joyinside_err_t;

/**
 * @brief  Events reported through the JoyInside event callback
 */
typedef enum {
    JOYINSIDE_EVENT_CONNECTED    = 0,  /*!< WebSocket connected and handshake done */
    JOYINSIDE_EVENT_DISCONNECTED = 1,  /*!< Connection closed */
    JOYINSIDE_EVENT_ERROR        = 2,  /*!< Transport or protocol error */
    JOYINSIDE_EVENT_ASR          = 3,  /*!< Partial ASR result available */
    JOYINSIDE_EVENT_ASR_FINAL    = 4,  /*!< Final ASR result for the utterance */
    JOYINSIDE_EVENT_TTS_COMPLETE = 5,  /*!< TTS playback stream finished */
    JOYINSIDE_EVENT_INTERRUPTED  = 6,  /*!< Server interrupted the current turn */
    JOYINSIDE_EVENT_CHAT_EXIT    = 7,  /*!< Voice chat session ended by server */
} joyinside_event_t;

/**
 * @brief  Opaque JoyInside client handle
 */
typedef struct joyinside *joyinside_handle_t;

/**
 * @brief  Lifecycle / status event callback
 *
 * @param[in]  event     Event identifier
 * @param[in]  user_ctx  Opaque pointer supplied in joyinside_config_t
 */
typedef void (*joyinside_event_cb_t)(joyinside_event_t event, void *user_ctx);

/**
 * @brief  Decoded TTS PCM callback (16 kHz, 16-bit, mono)
 *
 * @note   Called from the RX task; keep it short and do not block.
 *
 * @param[in]  pcm       Decoded PCM samples; valid only for the call duration
 * @param[in]  samples   Number of int16_t samples in pcm
 * @param[in]  user_ctx  Opaque pointer supplied in joyinside_config_t
 */
typedef void (*joyinside_pcm_cb_t)(const int16_t *pcm, int samples, void *user_ctx);

/**
 * @brief  ASR / TTS text callback (UTF-8, NUL-terminated)
 *
 * @param[in]  is_asr    true for recognized user speech (ASR), false for TTS sentence text
 * @param[in]  text      NUL-terminated UTF-8 text; valid only for the call duration
 * @param[in]  user_ctx  Opaque pointer supplied in joyinside_config_t
 */
typedef void (*joyinside_text_cb_t)(bool is_asr, const char *text, void *user_ctx);

/**
 * @brief  JoyInside client configuration
 *
 *         Any credential string left NULL falls back to its Kconfig default.
 *         The struct is copied by joyinside_create(); the caller retains ownership
 *         of the pointed-to strings only until that call returns.
 */
typedef struct {
    uint32_t             version;            /*!< Set to JOYINSIDE_CONFIG_VERSION */
    const char          *uri;                /*!< Endpoint URI; NULL uses Kconfig default */
    const char          *access_key_id;      /*!< Application access key id */
    const char          *access_key_secret;  /*!< Application access key secret */
    const char          *bot_id;             /*!< Robot / bot identifier */
    const char          *device_token;       /*!< Device token for JWT auth */
    const char          *device_id;          /*!< Device id (did) */
    joyinside_event_cb_t event_cb;           /*!< Status event callback; may be NULL */
    joyinside_pcm_cb_t   pcm_cb;             /*!< Decoded TTS PCM callback; may be NULL */
    joyinside_text_cb_t  text_cb;            /*!< ASR / TTS text callback; may be NULL */
    void                *user_ctx;           /*!< Passed back to every callback */
    uint32_t             reserved[4];         /*!< Reserved for ABI growth; zero-init */
} joyinside_config_t;

/**
 * @brief  Create a JoyInside client instance
 *
 * @note   Does not open the network connection; call joyinside_connect() next.
 *
 * @param[in]  config  Configuration; caller retains ownership
 *
 * @return  Handle on success, NULL on invalid argument or OOM
 */
joyinside_handle_t joyinside_create(const joyinside_config_t *config);

/**
 * @brief  Destroy a JoyInside client and release all resources
 *
 * @note   Disconnects first if still connected. After return, handle is invalid.
 *
 * @param[in]  handle  Handle from joyinside_create(); may be NULL (no-op)
 */
void joyinside_destroy(joyinside_handle_t handle);

/**
 * @brief  Connect to the JoyInside cloud and start the RX/TX tasks
 *
 * @param[in]  handle  Handle from joyinside_create()
 *
 * @return
 *       - JOYINSIDE_ERR_OK           On success
 *       - JOYINSIDE_ERR_NOT_INIT     handle is NULL
 *       - JOYINSIDE_ERR_BUSY         Already connected
 *       - JOYINSIDE_ERR_INVALID_ARG  Missing credentials or URI build failed
 *       - JOYINSIDE_ERR_FAIL         WebSocket connect failed
 */
joyinside_err_t joyinside_connect(joyinside_handle_t handle);

/**
 * @brief  Disconnect from the JoyInside cloud
 *
 * @param[in]  handle  Handle from joyinside_create()
 *
 * @return  JOYINSIDE_ERR_OK on success, JOYINSIDE_ERR_NOT_INIT if handle is NULL
 */
joyinside_err_t joyinside_disconnect(joyinside_handle_t handle);

/**
 * @brief  Report whether the client is connected
 *
 * @param[in]  handle  Handle from joyinside_create()
 *
 * @return  1 if connected, 0 otherwise
 */
int joyinside_is_connected(joyinside_handle_t handle);

/**
 * @brief  Send the chat-update event that starts a voice turn
 *
 * @param[in]  handle  Handle from joyinside_create()
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_chat_update(joyinside_handle_t handle);

/**
 * @brief  Queue captured PCM for opus-encoded uplink (16 kHz, 16-bit, mono)
 *
 * @note   Non-blocking; drops the frame if the uplink queue is full.
 *
 * @param[in]  handle   Handle from joyinside_create()
 * @param[in]  pcm      PCM samples to send; copied internally
 * @param[in]  samples  Number of int16_t samples (must be one 60 ms frame: 960)
 *
 * @return  JOYINSIDE_ERR_OK on enqueue, error code otherwise
 */
joyinside_err_t joyinside_send_audio(joyinside_handle_t handle, const int16_t *pcm, int samples);

/**
 * @brief  Ask the server to interrupt the current response
 *
 * @param[in]  handle  Handle from joyinside_create()
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_interrupt(joyinside_handle_t handle);

/**
 * @brief  Request server-side text-to-speech of the given text
 *
 * @param[in]  handle  Handle from joyinside_create()
 * @param[in]  text    NUL-terminated UTF-8 text to speak
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_text_to_speech(joyinside_handle_t handle, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* __JOYINSIDE_H__ */
