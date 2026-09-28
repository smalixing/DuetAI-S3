/*
 * SPDX-License-Identifier: MIT
 * PageManager demo entry: factory, manager setup, inactivity monitor, boot.
 */
#include "page_manager_demo.h"
#include "lv_page_manager/page_factory.h"
#include "home_page.h"
#include "screensaver_page.h"
#include "settings_page.h"
#include "bsp_lcd.h"
#include <string.h>

/**********************
 *  STATIC VARIABLES
 **********************/
/** Single demo page manager instance (stack bottom = home page). */
static pm_manager_t s_manager;

/** Guard: set once the demo has been started. */
static bool s_started = false;

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Shared factory: dispatch page construction by class name.
 * @note   Forwards each known class name to its page constructor; returns
 *         NULL for unknown classes so Install() fails loudly.
 * @param  factory     The factory instance (unused).
 * @param  class_name  Class name identifying which page to construct.
 * @retval Pointer to a newly created page, or NULL if unsupported.
 */
static pm_page_base_t* demo_factory_create(pm_factory_t* factory, const char* class_name)
{
    pm_page_base_t* page = NULL;

    if (strcmp(class_name, "HomePage") == 0) {
        page = home_page_create(factory, class_name);
    } else if (strcmp(class_name, "ScreensaverPage") == 0) {
        page = screensaver_page_create(factory, class_name);
    } else if (strcmp(class_name, "SettingsPage") == 0) {
        page = settings_page_create(factory, class_name);
    }

    if (page == NULL) {
        LV_LOG_ERROR("demo factory: failed to create '%s'", class_name);
    }
    return page;
}

/**********************
 *  GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Start the PageManager demo: install pages and show the home page.
 * @note   Must be called in LVGL context (e.g. from the UI builder before
 *         the LVGL task starts). Idempotent: a second call is a no-op.
 *         Also turns the LCD backlight on so the demo is visible, and arms
 *         the screensaver inactivity monitor.
 * @param  None
 * @retval ESP_OK on success, ESP_FAIL if the demo could not be started.
 */
esp_err_t page_manager_demo_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    bsp_lcd_set_backlight_level(SETTINGS_DEFAULT_BACKLIGHT_PCT);

    static pm_factory_t s_factory = {
        .create = demo_factory_create,
        .user_ctx = NULL,
    };

    page_manager_init(&s_manager, &s_factory);
    page_manager_set_global_load_anim_type(&s_manager, PM_LOAD_ANIM_OVER_LEFT, 400, lv_anim_path_ease_out);

    bool ok = true;
    ok = ok && home_page_register_self(&s_manager);
    ok = ok && screensaver_page_register_self(&s_manager);
    ok = ok && settings_page_register_self(&s_manager);
    if (!ok) {
        LV_LOG_ERROR("page manager demo: failed to install demo pages");
        page_manager_deinit(&s_manager);
        return ESP_FAIL;
    }

    if (!screensaver_inactivity_monitor_start(&s_manager)) {
        LV_LOG_ERROR("page manager demo: failed to start inactivity monitor");
    }

    page_manager_push(&s_manager, "HomePage", NULL);
    s_started = true;
    return ESP_OK;
}
