/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include "esp_log.h"
#include "bsp_board.h"
#include "bsp_i2c.h"
#include "bsp_i2s.h"
#include "bsp_codec.h"
#include "esp32s3_dual_module.h"
#include "smart_plate.h"

static const char *TAG = "bsp boards";

/*!< Board compiled into this build. smart_plate is the active hardware
 *   (GC9307 LCD + GT911 touch). */
#define BSP_BOARD_ACTIVE       BOARD_SMART_PLATE

static const boards_info_t g_boards_info[] = {
    {BOARD_SMART_PLATE, "SMART_PLATE",  bsp_board_smart_plate_init,  bsp_board_smart_plate_power_ctrl, bsp_board_smart_plate_get_res_desc},
    {BOARD_S3_DUAL_MODULE, "S3_DUAL_MODULE",  bsp_board_s3_dual_module_init,  bsp_board_s3_dual_module_power_ctrl, bsp_board_dual_module_get_res_desc},
};
static boards_info_t *g_board = NULL;

static esp_err_t bsp_board_detect()
{
    /*!< Look up the active board in the table (boards_id_t values are not
     *   array indices). */
    for (size_t i = 0; i < sizeof(g_boards_info) / sizeof(g_boards_info[0]); i++) {
        if (g_boards_info[i].id == BSP_BOARD_ACTIVE) {
            g_board = (boards_info_t *)&g_boards_info[i];
            break;
        }
    }

    const board_res_desc_t *brd = g_board->board_get_res_desc();
    if (brd->FUNC_I2C_EN) {
        /*!< Initialize I2C bus, used for audio codec and IMU */
        bsp_i2c_init(I2C_NUM_0, 400 * 1000, brd->GPIO_I2C_SCL, brd->GPIO_I2C_SDA);
    }

    ESP_LOGI(TAG, "Detected board: [%s]", g_board->name);
    return ESP_OK;
}

const boards_info_t *bsp_board_get_info(void)
{
    return g_board;
}

const board_res_desc_t *bsp_board_get_description(void)
{
    return g_board->board_get_res_desc();
}

esp_err_t bsp_board_init(void)
{
    if (ESP_OK == bsp_board_detect()) {
        return g_board->board_init();
    }
    return ESP_FAIL;
}

esp_err_t bsp_board_power_ctrl(power_module_t module, bool on)
{
    if (g_board) {
        return g_board->board_power_ctrl(module, on);
    }
    return ESP_FAIL;
}
