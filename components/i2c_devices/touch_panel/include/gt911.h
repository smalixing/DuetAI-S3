/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    gt911_gesture_none         = 0x00,
    gt911_gesture_move_up      = 0x10,
    gt911_gesture_move_left    = 0x14,
    gt911_gesture_move_down    = 0x18,
    gt911_gesture_move_right   = 0x1c,
    gt911_gesture_zoom_in      = 0x48,
    gt911_gesture_zoom_out     = 0x49,
} gt911_gesture_t;

/**
 * @brief Init gt911 series touch panel
 * 
 * @return 
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t gt911_init(void);

/**
 * @brief Read touch point from gt911
 * 
 * @param touch_points_num Touch point number
 * @param x X coordinate
 * @param y Y coordinate
 * @return 
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t gt911_read_pos(uint8_t *touch_points_num, uint16_t *x, uint16_t *y);

/**
 * @brief Read guesture from gt911
 * 
 * @param gesture Gesture read from gt911
 * @return 
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t gt911_read_gesture(gt911_gesture_t *gesture);

#ifdef __cplusplus
}
#endif
