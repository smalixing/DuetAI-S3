/*
 * SPDX-License-Identifier: MIT
 * PageManager demo: screensaver page (auto-enter after inactivity, wake-on-touch clock).
 */
#ifndef PM_DEMO_SCREENSAVER_PAGE_H
#define PM_DEMO_SCREENSAVER_PAGE_H

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
/** Seconds of input inactivity before the screensaver is pushed. */
#define SCREENSAVER_INACTIVITY_PERIOD_S  15

/**********************
 *      TYPEDEFS
 **********************/
/** Screensaver page: idle clock display; any touch pops back. */
typedef struct {
    pm_page_base_t base;             /*!< Common base (must be the first member). */
    lv_obj_t* clock_label;           /*!< Big clock label refreshed by the timer. */
} screensaver_page_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * @brief  Register the screensaver page into the manager's pool.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool screensaver_page_register_self(pm_manager_t* mgr);

/**
 * @brief  Construct a screensaver page for the demo factory.
 * @param  factory     The factory instance (unused).
 * @param  class_name  Must equal "ScreensaverPage".
 * @retval Pointer to the new page's base, or NULL on failure.
 */
pm_page_base_t* screensaver_page_create(pm_factory_t* factory, const char* class_name);

/**
 * @brief  Start the global inactivity monitor for the screensaver.
 * @note   Must be called in LVGL context after the manager is set up. The
 *         monitor polls display inactivity and pushes the screensaver page
 *         after SCREENSAVER_INACTIVITY_PERIOD_S seconds without input.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool screensaver_inactivity_monitor_start(pm_manager_t* mgr);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* PM_DEMO_SCREENSAVER_PAGE_H */
