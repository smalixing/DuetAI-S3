/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_websocket_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_JOYINSIDE_CHAT_CONFIG_VERSION   1
#define ESP_JOYINSIDE_DEFAULT_WS_BASE_URL     "wss://joyinside.jd.com/soulmate/voiceCall/v4"

/**
 * @brief  Uplink and downlink audio codec negotiated in CLIENT_VOICE_CHAT_UPDATE
 */
typedef enum {
    ESP_JOYINSIDE_AUDIO_TYPE_PCM  = 0,  /*!< Raw PCM, 16-bit signed little-endian */
    ESP_JOYINSIDE_AUDIO_TYPE_OPUS = 1,  /*!< Opus frames over WebSocket binary */
} esp_joyinside_audio_type_t;

/**
 * @brief  Chat session events delivered through esp_joyinside_chat_event_callback_t
 */
typedef enum {
    ESP_JOYINSIDE_CHAT_EVENT_CONNECTED = 0,           /*!< WebSocket connected */
    ESP_JOYINSIDE_CHAT_EVENT_DISCONNECTED,            /*!< WebSocket disconnected */
    ESP_JOYINSIDE_CHAT_EVENT_ASR_UPDATE,              /*!< Partial ASR text in event_data */
    ESP_JOYINSIDE_CHAT_EVENT_ASR_FINAL,               /*!< Final ASR text in event_data */
    ESP_JOYINSIDE_CHAT_EVENT_TTS_SENTENCE_START,      /*!< TTS sentence text in event_data */
    ESP_JOYINSIDE_CHAT_EVENT_TTS_COMPLETE,            /*!< Server finished current TTS */
    ESP_JOYINSIDE_CHAT_EVENT_INTERRUPT,               /*!< Server interrupted playback */
    ESP_JOYINSIDE_CHAT_EVENT_CALL_AGENT_START,        /*!< Agent call started */
    ESP_JOYINSIDE_CHAT_EVENT_CALL_AGENT_INTERRUPTED,  /*!< Agent call interrupted */
    ESP_JOYINSIDE_CHAT_EVENT_CALL_INTENT_END,         /*!< Intent round ended */
    ESP_JOYINSIDE_CHAT_EVENT_VOICE_CHAT_EXIT,         /*!< Server closed the voice session */
    ESP_JOYINSIDE_CHAT_EVENT_ERROR,                   /*!< Protocol or transport error */
    ESP_JOYINSIDE_CHAT_EVENT_CUSTOM_EVENT,            /*!< Unhandled EVENT JSON in event_data */
    ESP_JOYINSIDE_CHAT_EVENT_WS_EVENT,                /*!< Raw esp_websocket_client event */
    ESP_JOYINSIDE_CHAT_EVENT_AUDIO_DATA,              /*!< Downlink audio payload */
} esp_joyinside_chat_event_t;

/**
 * @brief  WebSocket event wrapper passed with ESP_JOYINSIDE_CHAT_EVENT_WS_EVENT
 */
typedef struct {
    void                    *handle;   /*!< esp_websocket_client handle */
    esp_websocket_event_id_t event_id; /*!< Underlying WebSocket event ID */
} esp_joyinside_ws_event_t;

/**
 * @brief  Flexible audio payload used with ESP_JOYINSIDE_CHAT_EVENT_AUDIO_DATA
 */
typedef struct {
    int   len;    /*!< Number of valid bytes in data */
    char  data[]; /*!< Audio bytes; valid only for the callback duration */
} esp_joyinside_chat_audio_data_t;

typedef void (*esp_joyinside_chat_audio_callback_t)(const uint8_t *data, int len, void *ctx);
typedef void (*esp_joyinside_chat_event_callback_t)(esp_joyinside_chat_event_t event,
                                                    void *event_data,
                                                    void *ctx);

typedef void *esp_joyinside_chat_handle_t;

/**
 * @brief  Runtime configuration for esp_joyinside_chat_init()
 *
 *         String pointers are copied during init. Callback pointers are stored
 *         by value and invoked from the WebSocket client task.
 */
typedef struct {
    uint32_t                            struct_version;              /*!< Set to ESP_JOYINSIDE_CHAT_CONFIG_VERSION */
    char                               *ws_base_url;               /*!< WebSocket base URL without auth query */
    char                               *bot_id;                    /*!< JoyInside bot ID */
    char                               *access_key_id;             /*!< JoyInside access key ID */
    char                               *access_key_secret;         /*!< JoyInside access key secret */
    char                               *device_id;                 /*!< Stable device identifier (DID) */
    char                               *device_token;              /*!< Device token used for MCP JWT; may be NULL */
    char                               *mcp_label;                 /*!< MCP serverInfo label */
    esp_joyinside_audio_type_t          uplink_audio_type;         /*!< Uplink codec */
    esp_joyinside_audio_type_t          downlink_audio_type;       /*!< Downlink codec */
    int                                 sample_rate;               /*!< Audio sample rate in Hz */
    int                                 frame_size_ms;             /*!< Frame duration in milliseconds */
    int                                 voice_volume;              /*!< Voice volume sent in init event */
    int                                 websocket_connect_timeout; /*!< Connect wait timeout in ms; 0 uses Kconfig default */
    esp_joyinside_chat_audio_callback_t audio_callback;            /*!< Downlink binary audio callback */
    esp_joyinside_chat_event_callback_t event_callback;            /*!< Session event callback */
    void                               *audio_callback_ctx;      /*!< User context for audio_callback */
    void                               *event_callback_ctx;      /*!< User context for event_callback */
    uint32_t                            reserved[4];               /*!< Reserved for future fields */
} esp_joyinside_chat_config_t;

#define ESP_JOYINSIDE_CHAT_DEFAULT_CONFIG() {                        \
    .struct_version            = ESP_JOYINSIDE_CHAT_CONFIG_VERSION,  \
    .ws_base_url               = ESP_JOYINSIDE_DEFAULT_WS_BASE_URL,  \
    .bot_id                    = NULL,                               \
    .access_key_id             = NULL,                               \
    .access_key_secret         = NULL,                               \
    .device_id                 = NULL,                               \
    .device_token              = NULL,                               \
    .mcp_label                 = "esp-joyinside",                    \
    .uplink_audio_type         = ESP_JOYINSIDE_AUDIO_TYPE_OPUS,      \
    .downlink_audio_type       = ESP_JOYINSIDE_AUDIO_TYPE_OPUS,      \
    .sample_rate               = 16000,                              \
    .frame_size_ms             = 60,                                 \
    .voice_volume              = 1,                                  \
    .websocket_connect_timeout = 0,                                  \
    .audio_callback            = NULL,                               \
    .event_callback            = NULL,                               \
    .audio_callback_ctx        = NULL,                               \
    .event_callback_ctx        = NULL,                               \
    .reserved                  = {0},                                \
}

/**
 * @brief  Create a JoyInside voice chat session handle
 *
 * @note   Not thread-safe. Call once before esp_joyinside_chat_start().
 *         event_callback and audio_callback run in the WebSocket client task.
 *         Pointers passed through event_data and audio data are valid only for
 *         the duration of the callback.
 *
 * @param[in]   config   Session configuration; required string fields must be non-NULL
 * @param[out]  chat_hd  Created handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  config, chat_hd, or required fields are invalid
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_joyinside_chat_init(const esp_joyinside_chat_config_t *config,
                                  esp_joyinside_chat_handle_t *chat_hd);

/**
 * @brief  Destroy a chat session handle and release all owned resources
 *
 * @note   The handle is invalid after this call returns.
 *
 * @param[in]  chat_hd  Handle returned by esp_joyinside_chat_init()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 */
esp_err_t esp_joyinside_chat_deinit(esp_joyinside_chat_handle_t chat_hd);

/**
 * @brief  Connect to JoyInside and send CLIENT_VOICE_CHAT_UPDATE
 *
 * @note   Blocks until connected or websocket_connect_timeout expires.
 *         Not thread-safe for the same handle.
 *
 * @param[in]  chat_hd  Initialized chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 *       - ESP_ERR_TIMEOUT      WebSocket connect timed out
 */
esp_err_t esp_joyinside_chat_start(esp_joyinside_chat_handle_t chat_hd);

/**
 * @brief  Stop the active WebSocket session
 *
 * @param[in]  chat_hd  Chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 */
esp_err_t esp_joyinside_chat_stop(esp_joyinside_chat_handle_t chat_hd);

/**
 * @brief  Query whether the WebSocket transport is connected
 *
 * @param[in]  chat_hd  Chat handle
 *
 * @return true when connected, false otherwise
 */
bool esp_joyinside_chat_is_connected(esp_joyinside_chat_handle_t chat_hd);

/**
 * @brief  Send one uplink audio frame as a WebSocket binary message
 *
 * @note   May block up to the configured audio send timeout.
 *
 * @param[in]  chat_hd  Connected chat handle
 * @param[in]  data     Audio payload
 * @param[in]  len      Payload length in bytes
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_ERR_INVALID_STATE  WebSocket is not connected
 */
esp_err_t esp_joyinside_chat_send_audio_data(esp_joyinside_chat_handle_t chat_hd,
                                             const uint8_t *data,
                                             size_t len);

/**
 * @brief  Notify the server that uplink audio for the current turn has ended
 *
 * @param[in]  chat_hd  Connected chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 */
esp_err_t esp_joyinside_chat_send_audio_complete(esp_joyinside_chat_handle_t chat_hd);

/**
 * @brief  Send CLIENT_INTERRUPT to stop current server playback or generation
 *
 * @param[in]  chat_hd  Connected chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 */
esp_err_t esp_joyinside_chat_interrupt(esp_joyinside_chat_handle_t chat_hd);

#ifdef __cplusplus
}
#endif
