/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

char *joyinside_jwt_create(const char *did, const char *device_token, uint64_t timestamp_s);

#ifdef __cplusplus
}
#endif
