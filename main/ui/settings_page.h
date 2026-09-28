/*
 * SPDX-License-Identifier: MIT
 * PageManager demo: settings page (backlight slider, switches, dropdown, back).
 */
#ifndef PM_DEMO_SETTINGS_PAGE_H
#define PM_DEMO_SETTINGS_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_page_manager/page_manager.h"
#include "lv_page_manager/page_base.h"
#include "lv_page_manager/page_factory.h"

/*********************
 *      DEFINES
 *********************/
/** Backlight percentage applied on demo start. */
#define SETTINGS_DEFAULT_BACKLIGHT_PCT  70

/**********************
 *      TYPEDEFS
 **********************/
/** Settings page: hardware-ish controls built on LVGL widgets. */
typedef struct {
    pm_page_base_t base;      /*!< Common base (must be the first member). */
} settings_page_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * @brief  Register the settings page into the manager's pool.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool settings_page_register_self(pm_manager_t* mgr);

/**
 * @brief  Construct a settings page for the demo factory.
 * @param  factory     The factory instance (unused).
 * @param  class_name  Must equal "SettingsPage".
 * @retval Pointer to the new page's base, or NULL on failure.
 */
pm_page_base_t* settings_page_create(pm_factory_t* factory, const char* class_name);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* PM_DEMO_SETTINGS_PAGE_H */
