/*
 * SPDX-License-Identifier: MIT
 * PageManager demo: home page (navigation hub / stack bottom).
 */
#ifndef PM_DEMO_HOME_PAGE_H
#define PM_DEMO_HOME_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_page_manager/page_manager.h"
#include "lv_page_manager/page_base.h"

/**********************
 *      TYPEDEFS
 **********************/
/** Home page: title + buttons that navigate to the other demo pages. */
typedef struct {
    pm_page_base_t base;      /*!< Common base (must be the first member). */
} home_page_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * @brief  Register the home page into the manager's pool.
 * @note   Creates one "HomePage" instance via the shared demo factory.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool home_page_register_self(pm_manager_t* mgr);

/**
 * @brief  Construct a home page for the demo factory.
 * @note   Allocates and base-initialises the page; the caller owns it.
 * @param  factory     The factory instance (unused).
 * @param  class_name  Must equal "HomePage".
 * @retval Pointer to the new page's base, or NULL on failure.
 */
pm_page_base_t* home_page_create(pm_factory_t* factory, const char* class_name);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* PM_DEMO_HOME_PAGE_H */
