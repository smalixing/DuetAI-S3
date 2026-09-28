/*
 * SPDX-License-Identifier: MIT
 * PageManager demo: screensaver page (auto-enter after inactivity, wake-on-touch clock).
 */
#include "screensaver_page.h"
#include <string.h>
#include <time.h>

/*********************
 *      DEFINES
 *********************/
#define SCREENSAVER_TIME_TOP_PX   60   /*!< Clock label offset from screen top. */
#define SCREENSAVER_FADE_TIME_MS  400  /*!< Fade duration when entering/leaving. */
#define SCREENSAVER_TICK_MS       1000 /*!< Clock refresh period. */
#define SCREENSAVER_POLL_MS       1000 /*!< Inactivity poll period. */

/**********************
 * STATIC PROTOTYPES
 **********************/
static void screensaver_on_custom_attr_config(pm_page_base_t* self);
static void screensaver_on_view_load(pm_page_base_t* self);
static void screensaver_on_destroy(pm_page_base_t* self);
static void screensaver_on_view_did_appear(pm_page_base_t* self);
static void screensaver_on_view_will_disappear(pm_page_base_t* self);
static void screensaver_on_view_unload(pm_page_base_t* self);
static void screensaver_clock_timer_cb(lv_timer_t* timer);
static void screensaver_root_pressed_cb(lv_event_t* e);
static void screensaver_inactivity_timer_cb(lv_timer_t* timer);

/**********************
 *  STATIC VARIABLES
 **********************/
/** Screensaver page life-cycle callbacks. */
static const pm_page_vtable_t s_screensaver_vtable = {
    .on_custom_attr_config  = screensaver_on_custom_attr_config,
    .on_view_load           = screensaver_on_view_load,
    .on_destroy             = screensaver_on_destroy,
    .on_view_did_appear     = screensaver_on_view_did_appear,
    .on_view_will_disappear = screensaver_on_view_will_disappear,
    .on_view_unload         = screensaver_on_view_unload,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Configure page attributes once after installation.
 * @note   Selects a fade-in load animation: a screensaver should dissolve in
 *         rather than slide over the previous page.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void screensaver_on_custom_attr_config(pm_page_base_t* self)
{
    page_base_set_custom_load_anim_type(self, PM_LOAD_ANIM_FADE_ON,
                                        SCREENSAVER_FADE_TIME_MS, lv_anim_path_ease_in);
}

/**
 * @brief  Build the screensaver UI: full-screen black background with a clock.
 * @note   The root is made clickable so any press wakes the page up. The
 *         clock label handle is kept in the page struct; the refresh timer
 *         is created in on_view_did_appear and stopped on disappear.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void screensaver_on_view_load(pm_page_base_t* self)
{
    screensaver_page_t* page = (screensaver_page_t*)self;
    lv_obj_set_size(self->root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(self->root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(self->root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(self->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(self->root, screensaver_root_pressed_cb, LV_EVENT_PRESSED, self);

    lv_obj_t* clock_label = lv_label_create(self->root);
    lv_obj_set_style_text_color(clock_label, lv_color_white(), LV_PART_MAIN);
    /* Use the 32 px font if enabled in sdkconfig, otherwise the default 14 px. */
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_32, LV_PART_MAIN);
#endif
    lv_obj_set_style_text_align(clock_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(clock_label, lv_pct(100));
    lv_obj_align(clock_label, LV_ALIGN_TOP_MID, 0, SCREENSAVER_TIME_TOP_PX);
    page->clock_label = clock_label;

    lv_obj_t* hint = lv_label_create(self->root);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(hint, "Tap to wake");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
}

/**
 * @brief  Start the clock refresh timer when the page is fully visible.
 * @note   The timer handle lives in user_data; the first tick is run manually
 *         so the clock is painted immediately instead of after one second.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void screensaver_on_view_did_appear(pm_page_base_t* self)
{
    lv_timer_t* timer = lv_timer_create(screensaver_clock_timer_cb, SCREENSAVER_TICK_MS, self);
    if (timer == NULL) {
        return;
    }
    self->user_data = timer;
    screensaver_clock_timer_cb(timer);
}

/**
 * @brief  Stop the clock timer before the disappear animation starts.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void screensaver_on_view_will_disappear(pm_page_base_t* self)
{
    if (self->user_data != NULL) {
        lv_timer_delete((lv_timer_t*)self->user_data);
        self->user_data = NULL;
    }
}

/**
 * @brief  Safety net: also stop the timer if the page is force-unloaded.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void screensaver_on_view_unload(pm_page_base_t* self)
{
    if (self->user_data != NULL) {
        lv_timer_delete((lv_timer_t*)self->user_data);
        self->user_data = NULL;
    }
}

/**
 * @brief  Periodic clock label refresh.
 * @note   Runs once per second in the LVGL timer context; reads wall time
 *         via time()/localtime_r() (system time without timezone handling).
 * @param  timer  Pointer to the LVGL timer.
 * @retval None
 */
static void screensaver_clock_timer_cb(lv_timer_t* timer)
{
    screensaver_page_t* page = (screensaver_page_t*)lv_timer_get_user_data(timer);

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    lv_label_set_text_fmt(page->clock_label, "%02d:%02d:%02d",
                          tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
}

/**
 * @brief  Root press handler: wake up by popping back to the previous page.
 * @note   Runs in the LVGL input context; the pop runs a quick fade-out.
 * @param  e  Pointer to the event descriptor.
 * @retval None
 */
static void screensaver_root_pressed_cb(lv_event_t* e)
{
    pm_page_base_t* self = (pm_page_base_t*)lv_event_get_user_data(e);
    page_base_set_custom_load_anim_type(self, PM_LOAD_ANIM_FADE_ON, SCREENSAVER_FADE_TIME_MS, lv_anim_path_ease_in);
    page_manager_pop(self->manager);
}

/**
 * @brief  Inactivity monitor: push the screensaver after an idle period.
 * @note   Polls the display inactivity time once per second. The screensaver
 *         is not pushed again while it is already on the stack.
 * @param  timer  Pointer to the LVGL timer.
 * @retval None
 */
static void screensaver_inactivity_timer_cb(lv_timer_t* timer)
{
    pm_manager_t* mgr = (pm_manager_t*)lv_timer_get_user_data(timer);

    if (page_manager_check_pages_exist(mgr, "ScreensaverPage")) {
        return;
    }
    if (lv_display_get_inactive_time(NULL) < SCREENSAVER_INACTIVITY_PERIOD_S * 1000) {
        return;
    }
    page_manager_push(mgr, "ScreensaverPage", NULL);
}

/**
 * @brief  Release the page object itself.
 * @note   Called by uninstall; frees the lv_malloc'd screensaver_page_t.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void screensaver_on_destroy(pm_page_base_t* self)
{
    lv_free(self);
}

/**********************
 *  GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Register the screensaver page into the manager's pool.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool screensaver_page_register_self(pm_manager_t* mgr)
{
    return page_manager_install(mgr, "ScreensaverPage", "ScreensaverPage");
}

/**
 * @brief  Construct a screensaver page for the demo factory.
 * @param  factory     The factory instance (unused).
 * @param  class_name  Must equal "ScreensaverPage".
 * @retval Pointer to the new page's base, or NULL on failure.
 */
pm_page_base_t* screensaver_page_create(pm_factory_t* factory, const char* class_name)
{
    LV_UNUSED(factory);
    if (strcmp(class_name, "ScreensaverPage") != 0) {
        return NULL;
    }
    screensaver_page_t* page = (screensaver_page_t*)lv_malloc(sizeof(screensaver_page_t));
    if (page == NULL) {
        return NULL;
    }
    page_base_init(&page->base, &s_screensaver_vtable);
    return &page->base;
}

/**
 * @brief  Start the global inactivity monitor for the screensaver.
 * @note   Must be called in LVGL context after the manager is set up. The
 *         monitor polls display inactivity and pushes the screensaver page
 *         after SCREENSAVER_INACTIVITY_PERIOD_S seconds without input.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool screensaver_inactivity_monitor_start(pm_manager_t* mgr)
{
    lv_timer_t* timer = lv_timer_create(screensaver_inactivity_timer_cb, SCREENSAVER_POLL_MS, mgr);
    return timer != NULL;
}
