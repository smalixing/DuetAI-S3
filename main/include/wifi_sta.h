/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Initialize the WiFi station and connect to the configured AP.
 *
 *         Brings up NVS, the TCP/IP stack, and the WiFi driver, then connects
 *         to the SSID/PSK defined in wifi_sta.c. Blocks until an IP address is
 *         obtained or the attempt times out. After a successful return the
 *         station keeps auto-reconnecting on link loss.
 *
 * @return
 *       - ESP_OK             Connected and got an IP address
 *       - ESP_ERR_TIMEOUT    Did not obtain an IP before the timeout
 *       - Others             NVS / netif / WiFi bring-up failed
 */
esp_err_t wifi_sta_start(void);

#ifdef __cplusplus
}
#endif
