/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_app_desc.h"
#include "esp_private/esp_clk.h"
#if CONFIG_SPIRAM
#include "esp_psram.h"
#endif

#include "bsp_board.h"
#include "bsp_lcd.h"
#include "lvgl_port.h"

#include "version.h"
#include "hal_log.h"

/**
 * @brief Print the startup banner with build and hardware info.
 *
 * Uses printf() rather than the logging macros so the ASCII art renders
 * without per-line log tags/timestamps.
 */
static void print_banner(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    const esp_app_desc_t *app_desc = esp_app_get_description();

    printf("\n");
    printf("     _             ___           _     _\n");
    printf("    | | ___  _   _|_ _|_ __  ___(_) __| | ___\n");
    printf(" _  | |/ _ \\| | | || || '_ \\/ __| |/ _` |/ _ \\\n");
    printf("| |_| | (_) | |_| || || | | \\__ \\ | (_| |  __/\n");
    printf(" \\___/ \\___/ \\__, |___|_| |_|___/_|\\__,_|\\___|\n");
    printf("             |___/\n");
    printf("\n");
    printf("------------------------------------------------------------\n");
    printf("Version     : %s\n", FW_VERSION_STRING);
    printf("SDK         : ESP-IDF %s\n", esp_get_idf_version());
    printf("Git         : %s\n", app_desc->version);
    printf("Build       : %s %s\n", app_desc->date, app_desc->time);
    printf("Chip        : %s Rev%d\n", CONFIG_IDF_TARGET, chip_info.revision);
    if (flash_size) {
        printf("Flash       : %" PRIu32 " MB\n", flash_size / (uint32_t)(1024 * 1024));
    }
#if CONFIG_SPIRAM
    printf("PSRAM       : %u MB\n", (unsigned)(esp_psram_get_size() / (1024 * 1024)));
#else
    printf("PSRAM       : none\n");
#endif
    printf("Heap Free   : %" PRIu32 " KB\n", esp_get_free_heap_size() / 1024);
    printf("CPU Freq    : %d MHz\n", esp_clk_cpu_freq() / 1000000);
    printf("------------------------------------------------------------\n");
    printf("\n");
}

void app_main(void)
{
    print_banner();

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
    /*!< Diagnostic: read back the actual LEDC duty as backlight percentage */
    hal_log_info("Backlight level: %d", bsp_lcd_get_backlight_level());

    while (1) {
        printf("free heap size: %ld, internal size: %ld, minimum size: %ld\n",
            esp_get_free_heap_size(),
            esp_get_free_internal_heap_size(),
            esp_get_minimum_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
