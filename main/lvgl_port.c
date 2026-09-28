/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "bsp_lcd.h"
#include "bsp_board.h"
#include "gt911.h"
#include "page_manager_demo.h"
#include "lvgl_port.h"

static const char *TAG = "lvgl_port";

/*!< Partial draw buffer: 1/6 of the screen, in DMA-capable memory */
#define LVGL_BUF_LINES  (40)

#define LVGL_TASK_STACK     (6 * 1024)
#define LVGL_TASK_PRIO      (2)
#define LVGL_TASK_PERIOD_MS (10)

static uint32_t lvgl_tick_get_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    /* SPI LCD panels (GC9307/ST7789/...) expect big-endian RGB565 */
    lv_draw_sw_rgb565_swap(px_map, w * h);

    /* esp_lcd draw_bitmap uses exclusive end coordinates -> +1 */
    bsp_lcd_flush(area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map, portMAX_DELAY);
    /* flush_ready is signalled from the LCD transfer-done callback below */
}

static bool lvgl_flush_ready_cb(void *user_data)
{
    lv_display_t *disp = (lv_display_t *)user_data;
    lv_display_flush_ready(disp);
    return false;
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_PERIOD_MS));
    }
}

/**
 * @brief GT911 touch read callback for LVGL.
 *
 * Keeps the last reported point so LVGL still has a valid coordinate when
 * the panel signals release (gt911_read_pos zeroes x/y on release) or when
 * a read fails (status register not ready yet).
 */
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;
    uint8_t points = 0;
    uint16_t x = last_x;
    uint16_t y = last_y;
    const board_res_desc_t *brd = bsp_board_get_description();

    if (gt911_read_pos(&points, &x, &y) == ESP_OK && points > 0) {
        last_x = x;
        last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    /* Map the raw panel coordinates onto the display orientation */
    if (brd->TOUCH_PANEL_SWAP_XY) {
        uint16_t t = x;
        x = y;
        y = t;
    }
    if (brd->TOUCH_PANEL_INVERSE_X) {
        x = brd->LCD_WIDTH - 1 - x;
    }
    if (brd->TOUCH_PANEL_INVERSE_Y) {
        y = brd->LCD_HEIGHT - 1 - y;
    }

    data->point.x = x;
    data->point.y = y;
}

static void lvgl_create_demo_ui(void)
{
    /* Page manager demo: home page + screensaver + settings */
    esp_err_t ret = page_manager_demo_start();
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Page manager demo failed to start");
    }
}

/**
 * @brief Register the GT911 touch panel as LVGL pointer input device.
 *
 * The GT911 sits on I2C0, which is brought up by bsp_board_init(); its
 * address-select reset sequence is done in the board init as well.
 */
static esp_err_t lvgl_touch_init(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    if (!brd->BSP_INDEV_IS_TP) {
        return ESP_OK; /* No touch panel on this board */
    }

    esp_err_t ret = gt911_init();
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "gt911 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    lv_indev_t *indev = lv_indev_create();
    if (NULL == indev) {
        ESP_LOGE(TAG, "Failed to create LVGL input device");
        return ESP_FAIL;
    }
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);

    return ESP_OK;
}

esp_err_t lvgl_port_init(void)
{
    const board_res_desc_t *brd = bsp_board_get_description();
    const int lcd_h_res = brd->LCD_WIDTH;
    const int lcd_v_res = brd->LCD_HEIGHT;
    const size_t buf_px = (size_t)lcd_h_res * LVGL_BUF_LINES;

    lv_init();
    lv_tick_set_cb(lvgl_tick_get_cb);

    lv_display_t *disp = lv_display_create(lcd_h_res, lcd_v_res);
    if (NULL == disp) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return ESP_FAIL;
    }

    size_t buf_bytes = buf_px * sizeof(lv_color16_t);
    void *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    void *buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA);
    if (NULL == buf1 || NULL == buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers");
        free(buf1);
        free(buf2);
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    /* Signal LVGL when the async LCD transfer completes */
    bsp_lcd_set_cb(lvgl_flush_ready_cb, disp);

    /* Touch input (non-fatal: the UI still works without it) */
    esp_err_t ret = lvgl_touch_init();
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "Touch input unavailable: %s", esp_err_to_name(ret));
    }

    /* Build UI before the handler task starts, so no locking is needed */
    lvgl_create_demo_ui();

    BaseType_t task_ret = xTaskCreate(lvgl_task, "lvgl", LVGL_TASK_STACK, NULL, LVGL_TASK_PRIO, NULL);
    if (pdPASS != task_ret) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL port initialized (%dx%d)", lcd_h_res, lcd_v_res);
    return ESP_OK;
}
