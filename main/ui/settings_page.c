/*
 * SPDX-License-Identifier: MIT
 * PageManager demo: settings page (backlight slider, switches, dropdown, back).
 */
#include "settings_page.h"
#include "bsp_lcd.h"
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define SETTINGS_ROW_GAP_PX        14  /*!< Vertical gap between rows. */
#define SETTINGS_TITLE_TOP_PX      8   /*!< Gap between screen top and header. */
#define SETTINGS_BTN_WIDTH_PX      150 /*!< Back button width. */
#define SETTINGS_BTN_HEIGHT_PX     36  /*!< Back button height. */

/**********************
 * STATIC PROTOTYPES
 **********************/
static void settings_on_view_load(pm_page_base_t* self);
static void settings_on_destroy(pm_page_base_t* self);
static void settings_back_btn_event_cb(lv_event_t* e);
static void settings_slider_event_cb(lv_event_t* e);
static void settings_switch_event_cb(lv_event_t* e);
static void settings_dropdown_event_cb(lv_event_t* e);

/**********************
 *  STATIC VARIABLES
 **********************/
/** Settings page life-cycle callbacks. */
static const pm_page_vtable_t s_settings_vtable = {
    .on_view_load = settings_on_view_load,
    .on_destroy   = settings_on_destroy,
};

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  Create one settings row: caption label on the left, control on the right.
 * @note   Rows use a horizontal flex container so caption and control align
 *         without manual coordinate arithmetic.
 * @param  parent    Parent container of the new row.
 * @param  caption   Row caption text (plain Latin, montserrat_14 only).
 * @retval Pointer to the created row object.
 */
static lv_obj_t* settings_row_create(lv_obj_t* parent, const char* caption)
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);

    lv_obj_t* label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(label, caption);
    return row;
}

/**
 * @brief  Build the settings page UI on the page root.
 * @note   The root is a scrollable column of rows: a Back button, a backlight
 *         slider wired to bsp_lcd_set_backlight_level(), a demo switch and a
 *         demo dropdown. The slider value is clamped into 0..100 percent.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void settings_on_view_load(pm_page_base_t* self)
{
    lv_obj_set_size(self->root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(self->root, lv_color_hex(0x222831), LV_PART_MAIN);
    lv_obj_set_flex_flow(self->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(self->root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(self->root, SETTINGS_TITLE_TOP_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_row(self->root, SETTINGS_ROW_GAP_PX, LV_PART_MAIN);

    /* Back button pops back to the home page. */
    lv_obj_t* back_btn = lv_button_create(self->root);
    lv_obj_set_size(back_btn, SETTINGS_BTN_WIDTH_PX, SETTINGS_BTN_HEIGHT_PX);
    lv_obj_add_event_cb(back_btn, settings_back_btn_event_cb, LV_EVENT_CLICKED, self);
    lv_obj_set_style_radius(back_btn, 10, LV_PART_MAIN);

    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);

    /* Backlight row: slider 0..100 percent. */
    lv_obj_t* bl_row = settings_row_create(self->root, "Backlight");
    lv_obj_t* slider = lv_slider_create(bl_row);
    lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, bsp_lcd_get_backlight_level(), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, settings_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Demo switch row. */
    lv_obj_t* sw_row = settings_row_create(self->root, "Dark mode");
    lv_obj_t* sw = lv_switch_create(sw_row);
    lv_obj_add_event_cb(sw, settings_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Demo dropdown row. */
    lv_obj_t* dd_row = settings_row_create(self->root, "Language");
    lv_obj_t* dd = lv_dropdown_create(dd_row);
    lv_dropdown_set_options(dd, "English\nFran" "\xc3\xa7" "ais\nDeutsch");
    lv_obj_set_flex_grow(dd, 1);
    lv_obj_add_event_cb(dd, settings_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/**
 * @brief  Release the page object itself.
 * @note   Called by uninstall; frees the lv_malloc'd settings_page_t.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void settings_on_destroy(pm_page_base_t* self)
{
    lv_free(self);
}

/**
 * @brief  Back button handler: leave the settings page.
 * @param  e  Pointer to the event descriptor.
 * @retval None
 */
static void settings_back_btn_event_cb(lv_event_t* e)
{
    pm_page_base_t* self = (pm_page_base_t*)lv_event_get_user_data(e);
    page_manager_pop(self->manager);
}

/**
 * @brief  Backlight slider handler: apply the new backlight percentage.
 * @param  e  Pointer to the event descriptor.
 * @retval None
 */
static void settings_slider_event_cb(lv_event_t* e)
{
    lv_obj_t* slider = lv_event_get_target_obj(e);
    int level = (int)lv_slider_get_value(slider);
    bsp_lcd_set_backlight_level(level);
}

/**
 * @brief  Demo switch handler: log the new checked state.
 * @param  e  Pointer to the event descriptor.
 * @retval None
 */
static void settings_switch_event_cb(lv_event_t* e)
{
    lv_obj_t* sw = lv_event_get_target_obj(e);
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
    LV_LOG_INFO("settings: dark mode %s", checked ? "on" : "off");
}

/**
 * @brief  Demo dropdown handler: log the newly selected option.
 * @param  e  Pointer to the event descriptor.
 * @retval None
 */
static void settings_dropdown_event_cb(lv_event_t* e)
{
    lv_obj_t* dd = lv_event_get_target_obj(e);
    uint16_t idx = lv_dropdown_get_selected(dd);
    LV_LOG_INFO("settings: language index %u", (unsigned)idx);
}

/**********************
 *  GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Register the settings page into the manager's pool.
 * @param  mgr  Pointer to the page manager.
 * @retval true on success, false on failure.
 */
bool settings_page_register_self(pm_manager_t* mgr)
{
    return page_manager_install(mgr, "SettingsPage", "SettingsPage");
}

/**
 * @brief  Construct a settings page for the demo factory.
 * @param  factory     The factory instance (unused).
 * @param  class_name  Must equal "SettingsPage".
 * @retval Pointer to the new page's base, or NULL on failure.
 */
pm_page_base_t* settings_page_create(pm_factory_t* factory, const char* class_name)
{
    LV_UNUSED(factory);
    if (strcmp(class_name, "SettingsPage") != 0) {
        return NULL;
    }
    settings_page_t* page = (settings_page_t*)lv_malloc(sizeof(settings_page_t));
    if (page == NULL) {
        return NULL;
    }
    page_base_init(&page->base, &s_settings_vtable);
    return &page->base;
}
