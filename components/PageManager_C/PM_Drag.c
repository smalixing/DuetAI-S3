/*
 * MIT License
 * C-language port of PageManager - drag gesture handling.
 *
 * This file implements the page drag functionality including:
 * - Root drag event handling
 * - Drag-based page navigation
 * - Inertia and gesture prediction
 */

#include "PageManager.h"
#include "PM_Log.h"
#include <stdlib.h>

#define CONSTRAIN(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

/* The distance threshold to trigger the drag */
#define PM_INDEV_DEF_DRAG_THROW 20

/* Forward declarations (definitions appear later in this file). */
static void on_root_drag_anim_finish(lv_anim_t* a);
static void on_root_async_leave(void* data);
static void pm_root_get_drag_predict(lv_coord_t* x, lv_coord_t* y);

/**
 * @brief Page drag event callback
 * @param event Pointer to event structure
 * @retval None
 */
static void on_root_drag_event(lv_event_t* event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    
    if (!(event_code == LV_EVENT_PRESSED || event_code == LV_EVENT_PRESSING || event_code == LV_EVENT_RELEASED)) {
        return;
    }

    lv_obj_t* root = lv_event_get_current_target(event);
    PageBase_t* base = (PageBase_t*)lv_event_get_user_data(event);
    
    if (base == NULL) {
        PM_LOG_ERROR("Page base is NULL");
        return;
    }

    PageManager_t* manager = base->manager;
    if (manager == NULL) {
        PM_LOG_ERROR("Page manager is NULL");
        return;
    }

    LoadAnimAttr_t anim_attr;
    if (!page_manager_get_current_load_anim_attr(manager, &anim_attr)) {
        PM_LOG_ERROR("Can't get current anim attr");
        return;
    }

    if (event_code == LV_EVENT_PRESSED) {
        if (manager->anim_state.is_switch_req) {
            return;
        }

        if (!manager->anim_state.is_busy) {
            return;
        }

        PM_LOG_INFO("Root anim interrupted");
        lv_anim_del(root, anim_attr.setter);
        manager->anim_state.is_busy = false;

        /* Temporary showing the bottom page */
        PageBase_t* bottom_page = page_manager_get_stack_top_after(manager);
        if (bottom_page) {
            lv_obj_clear_flag(bottom_page->root, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (event_code == LV_EVENT_PRESSING) {
        lv_coord_t cur = anim_attr.getter(root);

        lv_coord_t max = (anim_attr.pop.exit.start > anim_attr.pop.exit.end) ? 
                         anim_attr.pop.exit.start : anim_attr.pop.exit.end;
        lv_coord_t min = (anim_attr.pop.exit.start < anim_attr.pop.exit.end) ? 
                         anim_attr.pop.exit.start : anim_attr.pop.exit.end;

        lv_point_t offset;
        lv_indev_get_vect(lv_indev_get_act(), &offset);

        if (anim_attr.drag_dir == ROOT_DRAG_DIR_HOR) {
            cur += offset.x;
        } else if (anim_attr.drag_dir == ROOT_DRAG_DIR_VER) {
            cur += offset.y;
        }

        anim_attr.setter(root, CONSTRAIN(cur, min, max));
    } else if (event_code == LV_EVENT_RELEASED) {
        if (manager->anim_state.is_switch_req) {
            return;
        }

        lv_coord_t offset_sum = anim_attr.push.enter.end - anim_attr.push.enter.start;

        lv_coord_t x_predict = 0;
        lv_coord_t y_predict = 0;
        pm_root_get_drag_predict(&x_predict, &y_predict);

        lv_coord_t start = anim_attr.getter(root);
        lv_coord_t end = start;

        if (anim_attr.drag_dir == ROOT_DRAG_DIR_HOR) {
            end += x_predict;
            PM_LOG_INFO("Root drag x_predict = %d", end);
        } else if (anim_attr.drag_dir == ROOT_DRAG_DIR_VER) {
            end += y_predict;
            PM_LOG_INFO("Root drag y_predict = %d", end);
        }

        if ((end > 0 ? end : -end) > (offset_sum > 0 ? offset_sum : -offset_sum) / 2) {
            lv_async_call(on_root_async_leave, base);
        } else if (end != anim_attr.push.enter.end) {
            manager->anim_state.is_busy = true;

            lv_anim_t a;
            page_manager_anim_default_init(manager, &a);
            lv_anim_set_user_data(&a, manager);
            lv_anim_set_var(&a, root);
            lv_anim_set_values(&a, start, anim_attr.push.enter.end);
            lv_anim_set_exec_cb(&a, anim_attr.setter);
            lv_anim_set_ready_cb(&a, on_root_drag_anim_finish);
            lv_anim_start(&a);
            PM_LOG_INFO("Root drag anim start");
        }
    }
}

/**
 * @brief Drag animation end event callback
 * @param a Pointer to animation
 * @retval None
 */
static void on_root_drag_anim_finish(lv_anim_t* a)
{
    PageManager_t* manager = (PageManager_t*)lv_anim_get_user_data(a);
    PM_LOG_INFO("Root drag anim finish");
    manager->anim_state.is_busy = false;

    /* Hide the bottom page */
    PageBase_t* bottom_page = page_manager_get_stack_top_after(manager);
    if (bottom_page) {
        lv_obj_add_flag(bottom_page->root, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Enable root's drag function
 * @param self Pointer to page manager
 * @param root Pointer to the root object
 * @retval None
 */
void page_manager_root_enable_drag(PageManager_t* self, lv_obj_t* root)
{
    if (self == NULL || root == NULL) return;
    
    PageBase_t* base = (PageBase_t*)lv_obj_get_user_data(root);
    if (base == NULL) return;

    lv_obj_add_event_cb(
        root,
        on_root_drag_event,
        LV_EVENT_ALL,
        base
    );
    PM_LOG_INFO("Page(%s) Root drag enabled", base->name);
}

/**
 * @brief Asynchronous callback when dragging ends
 * @param data Pointer to the base class of the page
 * @retval None
 */
static void on_root_async_leave(void* data)
{
    PageBase_t* base = (PageBase_t*)data;
    PM_LOG_INFO("Page(%s) send event: LV_EVENT_LEAVE, need to handle...", base->name);
    lv_event_send(base->root, LV_EVENT_LEAVE, base);
}

/**
 * @brief Get drag inertia prediction stop point
 * @param x x stop point
 * @param y y stop point
 * @retval None
 */
static void pm_root_get_drag_predict(lv_coord_t* x, lv_coord_t* y)
{
    if (x == NULL || y == NULL) return;

    lv_indev_t* indev = lv_indev_get_act();
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    lv_coord_t y_predict = 0;
    lv_coord_t x_predict = 0;

    while (vect.y != 0) {
        y_predict += vect.y;
        vect.y = vect.y * (100 - PM_INDEV_DEF_DRAG_THROW) / 100;
    }

    while (vect.x != 0) {
        x_predict += vect.x;
        vect.x = vect.x * (100 - PM_INDEV_DEF_DRAG_THROW) / 100;
    }

    *x = x_predict;
    *y = y_predict;
}