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

#define JOYINSIDE_EVENT_ID_STR_LEN  (36u)
#define JOYINSIDE_EVENT_ID_BUF_LEN  (JOYINSIDE_EVENT_ID_STR_LEN + 1u)

esp_err_t joyinside_env_gen_event_id(char *out, size_t out_size);
char *joyinside_env_make_simple_event(const char *mid, const char *event_type);
char *joyinside_env_make_typed_json(const char *mid, const char *content_type, const char *content_json);
esp_err_t joyinside_env_ws_send_text_retry(esp_websocket_client_handle_t ws,
                                           const char *json,
                                           int len,
                                           int retries,
                                           int delay_ms,
                                           int timeout_ms,
                                           const char *op_name);
esp_err_t joyinside_env_ws_send_bin_retry(esp_websocket_client_handle_t ws,
                                          const uint8_t *data,
                                          int len,
                                          int retries,
                                          int delay_ms,
                                          int timeout_ms,
                                          const char *op_name);
esp_err_t joyinside_env_b64_decode_alloc(const char *b64, size_t b64_len,
                                         uint8_t **out_buf, size_t *out_len);

#ifdef __cplusplus
}
#endif
