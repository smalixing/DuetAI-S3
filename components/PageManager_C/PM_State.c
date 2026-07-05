/*
 * MIT License
 * C-language port of PageManager - page-state machine.
 */
#include "PageManager.h"
#include "PM_Log.h"

/**
 * @brief  Drive a page through its state machine.
 * @note   Central dispatcher of the page life cycle. Executes the handler for
 *         the page's current state and stores the returned next state. Some
 *         transitions are immediate and recurse (LOAD -> WILL_APPEAR,
 *         ACTIVITY -> WILL_DISAPPEAR, DID_DISAPPEAR -> UNLOAD) so the page
 *         advances as far as it can in one call; states that wait on an
 *         animation (WILL_APPEAR / WILL_DISAPPEAR) return and are resumed from
 *         the animation-finished callback. NULL @p base is ignored.
 * @param  self  Pointer to the page manager.
 * @param  base  Pointer to the page to advance (may be NULL).
 * @retval None
 */
void page_manager_state_update(PageManager_t* self, PageBase_t* base)
{
    if (self == NULL || base == NULL) return;

    switch (base->priv.state) {
    case PAGE_STATE_IDLE:
        PM_LOG_INFO("Page(%s) state idle", base->name);
        break;

    case PAGE_STATE_LOAD:
        base->priv.state = page_manager_state_load_execute(self, base);
        page_manager_state_update(self, base);
        break;

    case PAGE_STATE_WILL_APPEAR:
        base->priv.state = page_manager_state_will_appear_execute(self, base);
        break;

    case PAGE_STATE_DID_APPEAR:
        base->priv.state = page_manager_state_did_appear_execute(self, base);
        PM_LOG_INFO("Page(%s) state active", base->name);
        break;

    case PAGE_STATE_ACTIVITY:
        PM_LOG_INFO("Page(%s) state active break", base->name);
        base->priv.state = PAGE_STATE_WILL_DISAPPEAR;
        page_manager_state_update(self, base);
        break;

    case PAGE_STATE_WILL_DISAPPEAR:
        base->priv.state = page_manager_state_will_disappear_execute(self, base);
        break;

    case PAGE_STATE_DID_DISAPPEAR:
        base->priv.state = page_manager_state_did_disappear_execute(self, base);
        if (base->priv.state == PAGE_STATE_UNLOAD) {
            page_manager_state_update(self, base);
        }
        break;

    case PAGE_STATE_UNLOAD:
        base->priv.state = page_manager_state_unload_execute(self, base);
        break;

    default:
        PM_LOG_ERROR("Page(%s) state[%d] was NOT FOUND!", base->name, base->priv.state);
        break;
    }
}

/**
 * @brief  LOAD state handler: create the page's UI and prepare its cache flag.
 * @note   Creates the root lv_obj on the active screen, disables scrolling,
 *         links it back to the page via user-data, and applies the default
 *         root style if set. Calls on_view_load() then, for "over" animations
 *         where the page underneath is cached, enables drag-to-go-back on the
 *         new root. Calls on_view_did_load() and finally resolves the page's
 *         cache flag from its auto-cache configuration.
 * @param  self  Pointer to the page manager.
 * @param  base  Pointer to the page being loaded.
 * @retval Next state: PAGE_STATE_WILL_APPEAR.
 */
PageState_t page_manager_state_load_execute(PageManager_t* self, PageBase_t* base)
{
    PM_LOG_INFO("Page(%s) state load", base->name);

    if (base->root != NULL) {
        PM_LOG_ERROR("Page(%s) root must be NULL", base->name);
    }

    lv_obj_t* root_obj = lv_obj_create(lv_scr_act());
    lv_obj_clear_flag(root_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(root_obj, base);

    if (self->root_default_style) {
        lv_obj_add_style(root_obj, self->root_default_style, LV_PART_MAIN);
    }

    base->root = root_obj;
    PM_CALL_VFUNC(base, on_view_load);

    if (page_manager_get_is_over_anim(page_manager_get_current_load_anim_type(self))) {
        PageBase_t* bottom = page_manager_get_stack_top_after(self);
        if (bottom != NULL && bottom->priv.is_cached) {
            LoadAnimAttr_t attr;
            if (page_manager_get_current_load_anim_attr(self, &attr)) {
                if (attr.drag_dir != ROOT_DRAG_DIR_NONE) {
                    page_manager_root_enable_drag(self, base->root);
                }
            }
        }
    }

    PM_CALL_VFUNC(base, on_view_did_load);

    if (base->priv.is_disable_auto_cache) {
        PM_LOG_INFO("Page(%s) disable auto cache, req_enable_cache = %d",
                    base->name, base->priv.req_enable_cache);
        base->priv.is_cached = base->priv.req_enable_cache;
    } else {
        PM_LOG_INFO("Page(%s) AUTO cached", base->name);
        base->priv.is_cached = true;
    }

    return PAGE_STATE_WILL_APPEAR;
}

/**
 * @brief  WILL_APPEAR state handler: begin showing the page.
 * @note   Calls on_view_will_appear(), un-hides the page root, and starts the
 *         enter animation. The DID_APPEAR transition completes when that
 *         animation finishes.
 * @param  self  Pointer to the page manager.
 * @param  base  Pointer to the page about to appear.
 * @retval Next state: PAGE_STATE_DID_APPEAR.
 */
PageState_t page_manager_state_will_appear_execute(PageManager_t* self, PageBase_t* base)
{
    PM_LOG_INFO("Page(%s) state will appear", base->name);
    PM_CALL_VFUNC(base, on_view_will_appear);
    lv_obj_clear_flag(base->root, LV_OBJ_FLAG_HIDDEN);
    page_manager_switch_anim_create(self, base);
    return PAGE_STATE_DID_APPEAR;
}

/**
 * @brief  DID_APPEAR state handler: the page has finished appearing.
 * @note   Calls on_view_did_appear(); the page is now fully visible and active.
 * @param  self  Pointer to the page manager (unused).
 * @param  base  Pointer to the page that has appeared.
 * @retval Next state: PAGE_STATE_ACTIVITY.
 */
PageState_t page_manager_state_did_appear_execute(PageManager_t* self, PageBase_t* base)
{
    (void)self;
    PM_LOG_INFO("Page(%s) state did appear", base->name);
    PM_CALL_VFUNC(base, on_view_did_appear);
    return PAGE_STATE_ACTIVITY;
}

/**
 * @brief  WILL_DISAPPEAR state handler: begin hiding the page.
 * @note   Calls on_view_will_disappear() and starts the exit animation. The
 *         DID_DISAPPEAR transition completes when that animation finishes.
 * @param  self  Pointer to the page manager.
 * @param  base  Pointer to the page about to disappear.
 * @retval Next state: PAGE_STATE_DID_DISAPPEAR.
 */
PageState_t page_manager_state_will_disappear_execute(PageManager_t* self, PageBase_t* base)
{
    PM_LOG_INFO("Page(%s) state will disappear", base->name);
    PM_CALL_VFUNC(base, on_view_will_disappear);
    page_manager_switch_anim_create(self, base);
    return PAGE_STATE_DID_DISAPPEAR;
}

/**
 * @brief  DID_DISAPPEAR state handler: the page has finished hiding.
 * @note   Hides the root and calls on_view_did_disappear(). If the page is
 *         cached it stays loaded and returns to WILL_APPEAR (ready to show
 *         again without reloading); otherwise it proceeds to UNLOAD.
 * @param  self  Pointer to the page manager (unused).
 * @param  base  Pointer to the page that has disappeared.
 * @retval Next state: PAGE_STATE_WILL_APPEAR if cached, else PAGE_STATE_UNLOAD.
 */
PageState_t page_manager_state_did_disappear_execute(PageManager_t* self, PageBase_t* base)
{
    (void)self;
    PM_LOG_INFO("Page(%s) state did disappear", base->name);
    lv_obj_add_flag(base->root, LV_OBJ_FLAG_HIDDEN);
    PM_CALL_VFUNC(base, on_view_did_disappear);
    if (base->priv.is_cached) {
        PM_LOG_INFO("Page(%s) has cached", base->name);
        return PAGE_STATE_WILL_APPEAR;
    }
    return PAGE_STATE_UNLOAD;
}

/**
 * @brief  UNLOAD state handler: tear down the page's UI and free resources.
 * @note   Calls on_view_unload(), frees any stash buffer, asynchronously deletes
 *         the root object (so it can outlive the current event/animation),
 *         clears the cache flag and calls on_view_did_unload(). If the page was
 *         never loaded (root is NULL) it just returns to IDLE.
 * @param  self  Pointer to the page manager (unused).
 * @param  base  Pointer to the page being unloaded.
 * @retval Next state: PAGE_STATE_IDLE.
 */
PageState_t page_manager_state_unload_execute(PageManager_t* self, PageBase_t* base)
{
    (void)self;
    PM_LOG_INFO("Page(%s) state unload", base->name);
    if (base->root == NULL) {
        PM_LOG_WARN("Page is loaded!");
        return PAGE_STATE_IDLE;
    }

    PM_CALL_VFUNC(base, on_view_unload);

    if (base->priv.stash.ptr != NULL && base->priv.stash.size != 0) {
        PM_LOG_INFO("Page(%s) free stash(0x%p)[%d]",
                    base->name, base->priv.stash.ptr, (int)base->priv.stash.size);
        lv_mem_free(base->priv.stash.ptr);
        base->priv.stash.ptr  = NULL;
        base->priv.stash.size = 0;
    }

    /* Delete after the end of the root animation life cycle */
    lv_obj_del_async(base->root);
    base->root = NULL;
    base->priv.is_cached = false;
    PM_CALL_VFUNC(base, on_view_did_unload);

    return PAGE_STATE_IDLE;
}