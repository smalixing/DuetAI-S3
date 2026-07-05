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
 * @brief Initialize LVGL and bind it to the BSP LCD.
 *
 * Sets up the LVGL tick source, display, draw buffers, flush callback and a
 * dedicated task running lv_timer_handler(). A simple placeholder UI is created
 * so the screen shows content immediately.
 *
 * @note bsp_lcd_init() must have been called (done by bsp_board_init()) before
 *       calling this function.
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t lvgl_port_init(void);

#ifdef __cplusplus
}
#endif
