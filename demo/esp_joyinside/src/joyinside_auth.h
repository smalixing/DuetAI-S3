/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *joyinside_auth_generate_uri(const char *ws_base_url,
                                  const char *access_key_id,
                                  const char *access_key_secret,
                                  const char *bot_id);

#ifdef __cplusplus
}
#endif
