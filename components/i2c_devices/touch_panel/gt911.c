/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_log.h"
#include "bsp_i2c.h"
#include "bsp_board.h"
#include "gt911.h"
#include "bsp_gpio.h"

/** @brief gt911 register map and function codes */
#define gt911_ADDR            (0x5d)
#define GT911_CMD (0x8040)
#define GT911_VERSION (0x8047)
#define GT911_COODR (0x814E)
#define GT911_GESTURE (0x814d)



static i2c_bus_device_handle_t gt911_handle = NULL;

static inline esp_err_t gt911_read_byte(uint16_t reg_addr, uint8_t *data)
{
    return i2c_bus_read_reg16(gt911_handle, reg_addr,1,data);
}

static inline esp_err_t gt911_read_bytes(uint16_t reg_addr, size_t data_len, uint8_t *data)
{
    return i2c_bus_read_reg16(gt911_handle, reg_addr, data_len, data);
}

static inline esp_err_t gt911_write_byte(uint16_t reg_addr, uint8_t data)
{
    return i2c_bus_write_reg16(gt911_handle, reg_addr,1, &data);
}


esp_err_t gt911_init(void)
{
    if (NULL != gt911_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    bsp_i2c_add_device(I2C_NUM_0,&gt911_handle, gt911_ADDR);
    if (NULL == gt911_handle) {
        return ESP_FAIL;
    }

    esp_err_t ret_val = ESP_OK;
    uint8_t version = 0;
    
    gt911_write_byte(GT911_CMD,2);

    gt911_write_byte(GT911_CMD,0);

    gt911_read_byte(GT911_VERSION,&version);
    ESP_LOGI("gt911","get touch version: 0x%x",version);

    return ESP_OK;
}

static esp_err_t gt911_get_touch_points_num(uint8_t *touch_points_num)
{
    esp_err_t err;
    if((err = gt911_read_byte(GT911_COODR, touch_points_num)) == ESP_OK)
    {
        *touch_points_num = *touch_points_num&0x0f;
    }
    else
    {
        ESP_LOGE("gt911","get touch points num fail");
    }
    return err;
}

esp_err_t gt911_read_pos(uint8_t *touch_points_num, uint16_t *x, uint16_t *y)
{
    esp_err_t ret_val = ESP_FAIL;
    static uint16_t read_fail_count = 0;
    static uint16_t read_test_count = 0;
    static uint8_t data[8*5];
    uint8_t point_size = 0;

    if(gt911_read_bytes(GT911_COODR,8*5,data) == ESP_OK)
    {
        if(data[0]&0x80)
        {
            *touch_points_num = data[0]&0x0f;
            if (0 == *touch_points_num) {
                *x = 0;
                *y = 0;
            } else {
                *x = (data[2]&0xff) | ((data[3]<<8)&0xff00);
                *y = (data[4]&0xff) | ((data[5]<<8)&0xff00);
                point_size = (data[6]&0xff) | ((data[7]<<8)&0xff00);
                //ESP_LOGI("gt911","x:%d y:%d size:%d",*x,*y,point_size);
            }
            gt911_write_byte(GT911_COODR,0);
            ret_val = ESP_OK;
        }
        else
        {
            *touch_points_num = 0;
        }
        read_fail_count = 0;
        if(((read_test_count++)%500) == 0)
        {
            ESP_LOGI("gt911","x:%d y:%d size:%d",*x,*y,point_size);
        }
    }
    else
    {
        read_fail_count++;
        if(read_fail_count > 200)
        {
            uint8_t version = 0;
            const board_res_desc_t *brd = bsp_board_get_description();
            bsp_gpio_init(brd->TOUCH_REST_GPIO_NUM,GPIO_MODE_OUTPUT,0,GPIO_INTR_DISABLE);
            bsp_gpio_init(brd->TOUCH_IRQ_GPIO_NUM,GPIO_MODE_OUTPUT,0,GPIO_INTR_DISABLE);
            bsp_gpio_base_set(brd->TOUCH_REST_GPIO_NUM,0);
            bsp_gpio_base_set(brd->TOUCH_IRQ_GPIO_NUM,0);
            usleep(200);
            bsp_gpio_base_set(brd->TOUCH_REST_GPIO_NUM,1);
            usleep(5000);
            bsp_gpio_init(brd->TOUCH_IRQ_GPIO_NUM,GPIO_MODE_INPUT,1,GPIO_INTR_DISABLE);
            gt911_write_byte(GT911_CMD,2);
            gt911_write_byte(GT911_CMD,0);
            gt911_read_byte(GT911_VERSION,&version);
            read_fail_count = 0;
            ESP_LOGI("gt911","get touch version: 0x%x",version);
        }
    }
    

    return ret_val;
}

esp_err_t gt911_read_gesture(gt911_gesture_t *gesture)
{
    return gt911_read_byte(GT911_GESTURE, (uint8_t *)gesture);
}
