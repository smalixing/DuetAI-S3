/*
 * SPDX-FileCopyrightText: 2026 JD AIoT
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Firmware version. Bump these on release. */
#define FW_VERSION_MAJOR   1
#define FW_VERSION_MINOR   0
#define FW_VERSION_PATCH   0

#define FW_VERSION_STR__(a, b, c)  #a "." #b "." #c
#define FW_VERSION_STR_(a, b, c)   FW_VERSION_STR__(a, b, c)
/* Version as a string literal, e.g. "1.0.0" */
#define FW_VERSION_STRING \
    FW_VERSION_STR_(FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH)

#ifdef __cplusplus
}
#endif
