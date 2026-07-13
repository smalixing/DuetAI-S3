/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "psa/crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t joyinside_crypto_hmac(psa_algorithm_t hash_alg,
                                const uint8_t *key,
                                size_t key_len,
                                const uint8_t *input,
                                size_t input_len,
                                uint8_t *mac,
                                size_t mac_size,
                                size_t *mac_len);

#ifdef __cplusplus
}
#endif
