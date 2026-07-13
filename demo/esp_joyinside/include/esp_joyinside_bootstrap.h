/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_mac.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_JOYINSIDE_BOOTSTRAP_CONFIG_VERSION        1
#define ESP_JOYINSIDE_DEFAULT_GET_TOKEN_URL           "https://joyinside.jd.com/auth/getToken"
#define ESP_JOYINSIDE_DEFAULT_DEVICE_REGISTER_URL     "https://joyinside.jd.com/device/register"
#define ESP_JOYINSIDE_BOOTSTRAP_DEFAULT_TIMEOUT_MS    10000

/**
 * @brief  Parameters for JoyInside device registration
 *
 *         The returned bot ID belongs to the caller. Persist it in
 *         application-owned storage and pass it to esp_joyinside_chat_init()
 *         on later boots.
 */
typedef struct {
    uint32_t    struct_version;        /*!< Set to ESP_JOYINSIDE_BOOTSTRAP_CONFIG_VERSION */
    const char *access_key_id;         /*!< JoyInside access key ID */
    const char *access_key_secret;     /*!< JoyInside access key secret */
    const char *vendor_id;             /*!< JoyInside vendor ID */
    const char *app_id;                /*!< JoyInside application ID */
    const char *device_model;          /*!< Device model name */
    const char *device_id;             /*!< Stable device identifier */
    const char *get_token_url;         /*!< NULL uses ESP_JOYINSIDE_DEFAULT_GET_TOKEN_URL */
    const char *device_register_url;   /*!< NULL uses ESP_JOYINSIDE_DEFAULT_DEVICE_REGISTER_URL */
    int         timeout_ms;            /*!< HTTP timeout in ms; 0 uses default */
    uint32_t    reserved[4];           /*!< Reserved for future fields */
} esp_joyinside_bootstrap_config_t;

#define ESP_JOYINSIDE_BOOTSTRAP_DEFAULT_CONFIG() {          \
    .struct_version       = ESP_JOYINSIDE_BOOTSTRAP_CONFIG_VERSION, \
    .access_key_id        = NULL,                           \
    .access_key_secret    = NULL,                           \
    .vendor_id            = NULL,                           \
    .app_id               = NULL,                           \
    .device_model         = NULL,                           \
    .device_id            = NULL,                           \
    .get_token_url        = NULL,                           \
    .device_register_url   = NULL,                           \
    .timeout_ms           = 0,                              \
    .reserved             = {0},                            \
}

/**
 * @brief  Create a Base64 device ID from a selected eFuse MAC address
 *
 * @note   Caller owns the returned string and must free it with free().
 *
 * @param[in]   mac_type       MAC type accepted by esp_read_mac()
 * @param[out]  out_device_id  Allocated device ID string on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_device_id is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_joyinside_bootstrap_generate_device_id(esp_mac_type_t mac_type,
                                                     char **out_device_id);

/**
 * @brief  Acquire an access token and register the device
 *
 * @note   System time must be synchronized before calling this function because
 *         JoyInside V2 request signing includes a millisecond timestamp.
 *         Caller owns the returned bot ID and must free it with free().
 *
 * @param[in]   config      Bootstrap parameters
 * @param[out]  out_bot_id  Allocated bot ID string on success
 *
 * @return
 *       - ESP_OK                  On success
 *       - ESP_ERR_INVALID_ARG     Required fields are missing
 *       - ESP_ERR_INVALID_STATE   System time is not synchronized
 *       - ESP_ERR_NO_MEM          Allocation failed
 *       - ESP_FAIL                HTTP or server response error
 */
esp_err_t esp_joyinside_bootstrap_register(const esp_joyinside_bootstrap_config_t *config,
                                           char **out_bot_id);

#ifdef __cplusplus
}
#endif
