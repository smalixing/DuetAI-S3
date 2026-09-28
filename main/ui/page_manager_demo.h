/*
 * SPDX-License-Identifier: MIT
 * PageManager demo entry: factory, manager setup, inactivity monitor, boot.
 */
#ifndef PM_DEMO_PAGE_MANAGER_DEMO_H
#define PM_DEMO_PAGE_MANAGER_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "esp_err.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * @brief  Start the PageManager demo: install pages and show the home page.
 * @note   Must be called in LVGL context (e.g. from the UI builder before
 *         the LVGL task starts). Idempotent: a second call is a no-op.
 * @param  None
 * @retval ESP_OK on success, ESP_FAIL if the demo could not be started.
 */
esp_err_t page_manager_demo_start(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* PM_DEMO_PAGE_MANAGER_DEMO_H */
