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
    JOYINSIDE_EVENT_AUDIOBOOK_STOP = 8, /*!< Server asked to stop audiobook playback */
} joyinside_event_t;

/**
 * @brief  Audiobook playback request delivered by the server (contentType "AUDIO_BOOK")
 *
 *         Describes one chapter to stream. The audio is an mp3 URL (audio_url),
 *         so playback is left to the application: the SDK only relays this data
 *         through joyinside_audiobook_cb_t. All string pointers are valid only
 *         for the duration of the callback; copy anything you need to keep.
 */
typedef struct {
    const char *book_id;       /*!< Book identifier */
    const char *book_name;     /*!< Book display name; may be NULL */
    const char *chapter_id;    /*!< Chapter identifier */
    const char *chapter_name;  /*!< Chapter display name; may be NULL */
    const char *audio_url;     /*!< mp3 stream URL to play */
    const char *image_url;     /*!< Cover image URL; may be NULL */
    long        progress;      /*!< Resume position in seconds (0 if absent) */
    long        total_length;  /*!< Chapter total length in seconds (0 if absent) */
    bool        include_tts;   /*!< True if an interstitial TTS precedes playback */
} joyinside_audiobook_info_t;

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
 * @brief  Audiobook play-request callback
 *
 *         Fired when the server pushes an AUDIO_BOOK_PLAY request. The SDK does
 *         not play the mp3 itself; the application streams info->audio_url and,
 *         once playback actually starts, calls joyinside_audiobook_playing() to
 *         report it back and begin the keepalive ping. When playback ends (by
 *         the user or on natural completion) the application calls
 *         joyinside_audiobook_stopped().
 *
 * @note   Called from the RX task; keep it short and do not block. All pointers
 *         in info are valid only for the call duration.
 *
 * @param[in]  info      Chapter to play; borrowed for the call only
 * @param[in]  user_ctx  Opaque pointer supplied in joyinside_config_t
 */
typedef void (*joyinside_audiobook_cb_t)(const joyinside_audiobook_info_t *info, void *user_ctx);

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
    joyinside_audiobook_cb_t audiobook_cb;   /*!< Audiobook play-request callback; may be NULL */
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
 * @brief  Tell the server that uplink audio for this turn is complete
 *
 * @note   Send this once the user has stopped speaking so the server runs
 *         recognition and produces a response. Without it the server may keep
 *         waiting for more audio and never reply.
 *
 * @param[in]  handle  Handle from joyinside_create()
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_audio_finish(joyinside_handle_t handle);

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

/**
 * @brief  Send a typed user turn to the server as text (contentType "TEXT")
 *
 * @note   The server treats the text like a recognized utterance and replies
 *         with the usual ASR/TTS/EVENT stream. Use this for keyboard input in
 *         place of speaking a turn.
 *
 * @param[in]  handle  Handle from joyinside_create()
 * @param[in]  text    NUL-terminated UTF-8 text to send as user input
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_text_input(joyinside_handle_t handle, const char *text);

/**
 * @brief  Acknowledge that audiobook playback has started (uplink AUDIO_BOOK_PLAY)
 *
 *         Call this once the application has begun streaming the mp3 from the
 *         audio_url delivered in joyinside_audiobook_cb_t (after any interstitial
 *         TTS has finished). It also arms the internal keepalive ping.
 *
 * @param[in]  handle      Handle from joyinside_create()
 * @param[in]  book_id     Book identifier from the play request
 * @param[in]  chapter_id  Chapter identifier from the play request
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_audiobook_playing(joyinside_handle_t handle,
                                            const char *book_id, const char *chapter_id);

/**
 * @brief  Report that audiobook playback has stopped (uplink AUDIO_BOOK_STOP)
 *
 *         Call this when the user stops playback or a chapter finishes. On
 *         natural completion pass finish=true with progress set to the played
 *         length so the server can auto-advance to the next chapter. Stops the
 *         internal keepalive ping.
 *
 * @param[in]  handle      Handle from joyinside_create()
 * @param[in]  book_id     Book identifier from the play request
 * @param[in]  chapter_id  Chapter identifier from the play request
 * @param[in]  progress    Played length in seconds
 * @param[in]  finish      true if the chapter played to its natural end
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_audiobook_stopped(joyinside_handle_t handle,
                                            const char *book_id, const char *chapter_id,
                                            long progress, bool finish);

/**
 * @brief  Send an audiobook keepalive ping (uplink AUDIO_BOOK_PING)
 *
 * @note   The server expects a ping at least every 2 seconds while an audiobook
 *         is playing. Drive this from the application's playback loop.
 *
 * @param[in]  handle  Handle from joyinside_create()
 *
 * @return  JOYINSIDE_ERR_OK on success, error code otherwise
 */
joyinside_err_t joyinside_audiobook_ping(joyinside_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __JOYINSIDE_H__ */
