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
#include "demos/lv_demos.h"
#include "bsp_lcd.h"
#include "lvgl_port.h"

static const char *TAG = "lvgl_port";

#define LCD_H_RES       (240)
#define LCD_V_RES       (240)
/*!< Partial draw buffer: 1/6 of the screen, in DMA-capable memory */
#define LVGL_BUF_LINES  (40)
#define LVGL_BUF_PX     (LCD_H_RES * LVGL_BUF_LINES)

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

    /* ST7789 expects big-endian RGB565 */
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

static void lvgl_create_demo_ui(void)
{
    /* Run the official LVGL widgets demo */
    lv_demo_widgets();
}

esp_err_t lvgl_port_init(void)
{
    lv_init();
    lv_tick_set_cb(lvgl_tick_get_cb);

    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (NULL == disp) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return ESP_FAIL;
    }

    size_t buf_bytes = LVGL_BUF_PX * sizeof(lv_color16_t);
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

    /* Build UI before the handler task starts, so no locking is needed */
    lvgl_create_demo_ui();

    BaseType_t ret = xTaskCreate(lvgl_task, "lvgl", LVGL_TASK_STACK, NULL, LVGL_TASK_PRIO, NULL);
    if (pdPASS != ret) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL port initialized");
    return ESP_OK;
}
