/*
 * SPDX-License-Identifier: MIT
 * PageManager demo: home page (navigation hub / stack bottom).
 */
#include "home_page.h"
#include "lv_page_manager/page_factory.h"
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define HOME_TITLE_TOP_OFFSET_PX  20  /*!< Gap between screen top and title. */
#define HOME_ROW_GAP_PX           16  /*!< Vertical gap between buttons. */
#define HOME_BTN_WIDTH_PX         180 /*!< Navigation button width. */
#define HOME_BTN_HEIGHT_PX        44  /*!< Navigation button height. */

/**********************
 * STATIC PROTOTYPES
 **********************/
static void home_on_view_load(pm_page_base_t* self);
static void home_on_destroy(pm_page_base_t* self);
static void home_nav_btn_event_cb(lv_event_t* e);

/**********************
 *  STATIC VARIABLES
 **********************/
/** Home page life-cycle callbacks. */
static const pm_page_vtable_t s_home_vtable = {
    .on_view_load = home_on_view_load,
    .on_destroy   = home_on_destroy,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Build the home page UI on the page root.
 * @note   The root is set to full-screen size (LVGL defaults it to 130x130
 *         otherwise), then a title and the navigation buttons are laid out.
 *         Each button carries the target page name in its object user_data
 *         and the page base in the event user_data.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void home_on_view_load(pm_page_base_t* self)
{
    lv_obj_set_size(self->root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(self->root, lv_color_hex(0x222831), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(self->root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_flex_flow(self->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(self->root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(self->root, HOME_TITLE_TOP_OFFSET_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_row(self->root, HOME_ROW_GAP_PX, LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(self->root);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(title, LV_SYMBOL_WIFI " Page Manager Demo");
    lv_obj_set_style_pad_bottom(title, 12, LV_PART_MAIN);

    /* Static storage: button user_data keeps these name pointers alive. */
    static const struct {
        const char* name;   /*!< Target page application name. */
        const char* text;   /*!< Button label text. */
    } nav_items[] = {
        {"ScreensaverPage", LV_SYMBOL_PLAY     " Screensaver"},
        {"SettingsPage",    LV_SYMBOL_SETTINGS " Settings"},
    };

    for (size_t i = 0; i < sizeof(nav_items) / sizeof(nav_items[0]); i++) {
        lv_obj_t* btn = lv_button_create(self->root);
        lv_obj_set_size(btn, HOME_BTN_WIDTH_PX, HOME_BTN_HEIGHT_PX);
        lv_obj_set_user_data(btn, (void*)nav_items[i].name);
        lv_obj_add_event_cb(btn, home_nav_btn_event_cb, LV_EVENT_CLICKED, self);
        lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);

        lv_obj_t* label = lv_label_create(btn);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_label_set_text(label, nav_items[i].text);
        lv_obj_center(label);
    }
}

/**
 * @brief  Release the page object itself.
 * @note   Called by uninstall; frees the lv_malloc'd home_page_t.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void home_on_destroy(pm_page_base_t* self)
{
    lv_free(self);
}

/**
 * @brief  Navigation button click handler.
 * @note   The target page name comes from the button's object user_data and
 *         the owning page from the event user_data; the push runs in the
 *         LVGL context this callback already executes in.
 * @param  e  Pointer to the event descriptor.
 * @retval None
 */
static void home_nav_btn_event_cb(lv_event_t* e)
{
    pm_page_base_t* self = (pm_page_base_t*)lv_event_get_user_data(e);
    const char* target = (const char*)lv_obj_get_user_data(lv_event_get_target_obj(e));
    page_manager_push(self->manager, target, NULL);
}

/**********************
 *  GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Register the home page into the manager's pool.
 * @note   Creates one "HomePage" instance via the shared demo factory.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool home_page_register_self(pm_manager_t* mgr)
{
    return page_manager_install(mgr, "HomePage", "HomePage");
}

/**
 * @brief  Construct a home page for the demo factory.
 * @note   Allocates and base-initialises the page; the caller owns it.
 * @param  factory     The factory instance (unused).
 * @param  class_name  Must equal "HomePage".
 * @retval Pointer to the new page's base, or NULL on failure.
 */
pm_page_base_t* home_page_create(pm_factory_t* factory, const char* class_name)
{
    LV_UNUSED(factory);
    if (strcmp(class_name, "HomePage") != 0) {
        return NULL;
    }
    home_page_t* page = (home_page_t*)lv_malloc(sizeof(home_page_t));
    if (page == NULL) {
        return NULL;
    }
    page_base_init(&page->base, &s_home_vtable);
    return &page->base;
}
