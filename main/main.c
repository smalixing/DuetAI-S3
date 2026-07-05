/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_err.h"

#include "bsp_board.h"
#include "lvgl_port.h"

#include "hal_log.h"

static const char *TAG = "main";

void app_main(void)
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    hal_log_info("%s chip with %d CPU core(s)", CONFIG_IDF_TARGET, chip_info.cores);
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        hal_log_info("%" PRIu32 "MB %s flash",
                 flash_size / (uint32_t)(1024 * 1024),
                 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    }
    hal_log_info("Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());

    /* Board and peripheral hardware initialization */
    hal_log_info("Initializing board...");
    esp_err_t ret = bsp_board_init();
    if (ESP_OK != ret) {
        hal_log_err("Board init failed: %s", esp_err_to_name(ret));
        return;
    }
    hal_log_info("Board init done");

    /* Initialize LVGL and light up the screen */
    ret = lvgl_port_init();
    if (ESP_OK != ret) {
        hal_log_err("LVGL port init failed: %s", esp_err_to_name(ret));
        return;
    }
    hal_log_info("LVGL port init done");

    while (1) {
        printf("free heap size: %ld, internal size: %ld, minimum size: %ld\n",
            esp_get_free_heap_size(),
            esp_get_free_internal_heap_size(),
            esp_get_minimum_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
