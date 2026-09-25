/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Special config for smart_plate board
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t bsp_board_smart_plate_init(void);

/**
* @brief Control power of smart_plate board
*
* @param module Refer to `power_module_t`
* @param on Turn on or off specified power module. On if true
* @return
*    - ESP_OK: Success
*    - Others: Fail
*/
esp_err_t bsp_board_smart_plate_power_ctrl(power_module_t module, bool on);

/**
 * @brief Get smart_plate board description
 *
 * @return pointer of board_res_desc_t
 */
const board_res_desc_t *bsp_board_smart_plate_get_res_desc(void);

#ifdef __cplusplus
}
#endif
