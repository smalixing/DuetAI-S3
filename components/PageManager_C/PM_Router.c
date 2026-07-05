/*
 * MIT License
 * C-language port of PageManager - routing core (Push/Pop/Replace/SwitchTo).
 *
 * This file implements the page routing functionality including:
 * - Push/Pop/Replace page navigation
 * - SwitchTo core switching logic
 * - stash parameter passing
 * - Animation state management
 */

#include "PageManager.h"
#include "PM_Log.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Enter a new page, replace the old page
 * @param self Pointer to page manager
 * @param name The name of the page to enter
 * @param stash Parameters passed to the new page
 * @retval Return true if successful
 */
bool page_manager_replace(PageManager_t* self, const char* name, const PageStash_t* stash)
{
    /* Check whether the animation of switching pages is being executed */
    if (!page_manager_switch_anim_state_check(self)) {
        return false;
    }

    /* Check whether the stack is repeatedly pushed */
    if (page_manager_find_page_in_stack(self, name) != NULL) {
        PM_LOG_ERROR("Page(%s) was multi push", name);
        return false;
    }

    /* Check if the page is registered in the page pool */
    PageBase_t* base = page_manager_find_page_in_pool(self, name);
    if (base == NULL) {
        PM_LOG_ERROR("Page(%s) was not install", name);
        return false;
    }

    /* Get the top page of the stack */
    PageBase_t* top = page_manager_get_stack_top(self);
    if (top == NULL) {
        PM_LOG_ERROR("Stack top is NULL");
        return false;
    }

    /* Force disable cache */
    top->priv.is_cached = false;

    /* Synchronous automatic cache configuration */
    base->priv.is_disable_auto_cache = base->priv.req_disable_auto_cache;

    /* Remove current page */
    self->stack.size--;

    /* Push into the stack */
    if (!page_array_push(&self->stack, base)) {
        return false;
    }

    PM_LOG_INFO("Page(%s) replace Page(%s) (stash = 0x%p)", name, top->name, stash);

    /* Page switching execution */
    return page_manager_switch_to(self, base, true, stash);
}

/**
 * @brief Enter a new page, the old page is pushed onto the stack
 * @param self Pointer to page manager
 * @param name The name of the page to enter
 * @param stash Parameters passed to the new page
 * @retval Return true if successful
 */
bool page_manager_push(PageManager_t* self, const char* name, const PageStash_t* stash)
{
    /* Check whether the animation of switching pages is being executed */
    if (!page_manager_switch_anim_state_check(self)) {
        return false;
    }

    /* Check whether the stack is repeatedly pushed */
    if (page_manager_find_page_in_stack(self, name) != NULL) {
        PM_LOG_ERROR("Page(%s) was multi push", name);
        return false;
    }

    /* Check if the page is registered in the page pool */
    PageBase_t* base = page_manager_find_page_in_pool(self, name);
    if (base == NULL) {
        PM_LOG_ERROR("Page(%s) was not install", name);
        return false;
    }

    /* Synchronous automatic cache configuration */
    base->priv.is_disable_auto_cache = base->priv.req_disable_auto_cache;

    /* Push into the stack */
    if (!page_array_push(&self->stack, base)) {
        return false;
    }

    PM_LOG_INFO("Page(%s) push >> [Screen] (stash = 0x%p)", name, stash);

    /* Page switching execution */
    return page_manager_switch_to(self, base, true, stash);
}

/**
 * @brief Pop the current page
 * @param self Pointer to page manager
 * @retval Return true if successful
 */
bool page_manager_pop(PageManager_t* self)
{
    /* Check whether the animation of switching pages is being executed */
    if (!page_manager_switch_anim_state_check(self)) {
        return false;
    }

    /* Get the top page of the stack */
    PageBase_t* top = page_manager_get_stack_top(self);
    if (top == NULL) {
        PM_LOG_WARN("Page stack is empty, cat't pop");
        return false;
    }

    /* Whether to turn off automatic cache */
    if (!top->priv.is_disable_auto_cache) {
        PM_LOG_INFO("Page(%s) has auto cache, cache disabled", top->name);
        top->priv.is_cached = false;
    }

    PM_LOG_INFO("Page(%s) pop << [Screen]", top->name);

    /* Page popup */
    self->stack.size--;

    /* Get the next page */
    top = page_manager_get_stack_top(self);

    /* Page switching execution */
    return page_manager_switch_to(self, top, false, NULL);
}

/**
 * @brief Pop the current page but don't show
 * @param self Pointer to page manager
 * @retval Return true if successful
 */
bool page_manager_pop_unshow(PageManager_t* self)
{
    /* Check whether the animation of switching pages is being executed */
    if (!page_manager_switch_anim_state_check(self)) {
        return false;
    }

    /* Get the top page of the stack */
    PageBase_t* top = page_manager_get_stack_top(self);
    if (top == NULL) {
        PM_LOG_WARN("Page stack is empty, cat't pop");
        return false;
    }

    /* Whether to turn off automatic cache */
    if (!top->priv.is_disable_auto_cache) {
        PM_LOG_INFO("Page(%s) has auto cache, cache disabled", top->name);
        top->priv.is_cached = false;
    }

    PM_LOG_INFO("Page(%s) pop << [Screen]", top->name);

    /* Page popup */
    self->stack.size--;
    return true;
}

/**
 * @brief Page switching
 * @param self Pointer to page manager
 * @param new_node Pointer to new page
 * @param is_enter_act Whether it is a ENTER action
 * @param stash Parameters passed to the new page
 * @retval Return true if successful
 */
bool page_manager_switch_to(PageManager_t* self, PageBase_t* new_node, bool is_enter_act, const PageStash_t* stash)
{
    if (self == NULL || new_node == NULL) {
        PM_LOG_ERROR("self or new_node is nullptr");
        return false;
    }

    /* Whether page switching has been requested */
    if (self->anim_state.is_switch_req) {
        PM_LOG_WARN("Page switch busy, reqire(%s) is ignore", new_node->name);
        return false;
    }

    self->anim_state.is_switch_req = true;

    /* Is there a parameter to pass */
    if (stash != NULL) {
        PM_LOG_INFO("stash is detect, %s >> stash(0x%p) >> %s", 
                   page_manager_get_page_prev_name(self), stash, new_node->name);

        void* buffer = NULL;

        if (new_node->priv.stash.ptr == NULL) {
            buffer = lv_mem_alloc(stash->size);
            if (buffer == NULL) {
                PM_LOG_ERROR("stash malloc failed");
            } else {
                PM_LOG_INFO("stash(0x%p) malloc[%d]", buffer, stash->size);
            }
        } else if (new_node->priv.stash.size == stash->size) {
            buffer = new_node->priv.stash.ptr;
            PM_LOG_INFO("stash(0x%p) is exist", buffer);
        }

        if (buffer != NULL) {
            memcpy(buffer, stash->ptr, stash->size);
            PM_LOG_INFO("stash memcpy[%d] 0x%p >> 0x%p", stash->size, stash->ptr, buffer);
            new_node->priv.stash.ptr = buffer;
            new_node->priv.stash.size = stash->size;
        }
    }

    /* Record current page */
    self->page_current = new_node;

    /* If the current page has a cache */
    if (self->page_current->priv.is_cached) {
        /* Direct display, no need to load */
        PM_LOG_INFO("Page(%s) has cached, appear directly", self->page_current->name);
        self->page_current->priv.state = PAGE_STATE_WILL_APPEAR;
    } else {
        /* Load page */
        self->page_current->priv.state = PAGE_STATE_LOAD;
    }

    if (self->page_prev != NULL) {
        self->page_prev->priv.anim.is_enter = false;
    }

    self->page_current->priv.anim.is_enter = true;

    self->anim_state.is_entering = is_enter_act;

    if (self->anim_state.is_entering) {
        /* Update the animation configuration according to the current page */
        page_manager_switch_anim_type_update(self, self->page_current);
    }

    /* Update the state machine of the previous page */
    page_manager_state_update(self, self->page_prev);

    /* Update the state machine of the current page */
    page_manager_state_update(self, self->page_current);

    /* Move the layer, move the new page to the front */
    if (self->anim_state.is_entering) {
        PM_LOG_INFO("Page ENTER is detect, move Page(%s) to foreground", self->page_current->name);
        if (self->page_prev) lv_obj_move_foreground(self->page_prev->root);
        lv_obj_move_foreground(self->page_current->root);
    } else {
        PM_LOG_INFO("Page EXIT is detect, move Page(%s) to foreground", page_manager_get_page_prev_name(self));
        lv_obj_move_foreground(self->page_current->root);
        if (self->page_prev) lv_obj_move_foreground(self->page_prev->root);
    }

    return true;
}

/**
 * @brief Force the end of the life cycle of the page without animation
 * @param self Pointer to page manager
 * @param base Pointer to the page being executed
 * @retval Return true if successful
 */
bool page_manager_fource_unload(PageManager_t* self, PageBase_t* base)
{
    if (self == NULL || base == NULL) {
        PM_LOG_ERROR("Page is nullptr, Unload failed");
        return false;
    }

    PM_LOG_INFO("Page(%s) Fource unloading...", base->name);

    if (base->priv.state == PAGE_STATE_ACTIVITY) {
        PM_LOG_INFO("Page state is ACTIVITY, Disappearing...");
        PM_CALL_VFUNC(base, on_view_will_disappear);
        PM_CALL_VFUNC(base, on_view_did_disappear);
    }

    base->priv.state = page_manager_state_unload_execute(self, base);

    return true;
}

/**
 * @brief Back to the main page (the page at the bottom of the stack)
 * @param self Pointer to page manager
 * @retval Return true if successful
 */
bool page_manager_back_home(PageManager_t* self)
{
    /* Check whether the animation of switching pages is being executed */
    if (!page_manager_switch_anim_state_check(self)) {
        return false;
    }

    page_manager_set_stack_clear(self, true);

    self->page_prev = NULL;

    PageBase_t* home = page_manager_get_stack_top(self);

    return page_manager_switch_to(self, home, false, NULL);
}

bool page_manager_back_home_unshow(PageManager_t* self)
{
    /* Check whether the animation of switching pages is being executed */
    if (!page_manager_switch_anim_state_check(self)) {
        return false;
    }

    page_manager_set_stack_clear(self, true);

    self->page_prev = NULL;

    return true;
}

/**
 * @brief Check if the page switching animation is being executed
 * @param self Pointer to page manager
 * @retval Return true if it is executing
 */
bool page_manager_switch_anim_state_check(PageManager_t* self)
{
    if (self == NULL) return false;
    
    if (self->anim_state.is_switch_req || self->anim_state.is_busy) {
        PM_LOG_WARN(
            "Page switch busy[AnimState.is_switch_req = %d,"
            "AnimState.is_busy = %d],"
            "request ignored",
            self->anim_state.is_switch_req,
            self->anim_state.is_busy);
        return false;
    }

    return true;
}

/**
 * @brief Page switching request check
 * @param self Pointer to page manager
 * @retval Return true if all pages are executed
 */
bool page_manager_switch_req_check(PageManager_t* self)
{
    if (self == NULL) return false;
    
    bool ret = false;
    bool last_node_busy = (self->page_prev != NULL) && self->page_prev->priv.anim.is_busy;

    if (!self->page_current->priv.anim.is_busy && !last_node_busy) {
        PM_LOG_INFO("----Page switch was all finished----");
        self->anim_state.is_switch_req = false;
        ret = true;
        self->page_prev = self->page_current;
    } else {
        if (self->page_current->priv.anim.is_busy) {
            PM_LOG_WARN("Page PageCurrent(%s) is busy", self->page_current->name);
        } else {
            PM_LOG_WARN("Page PagePrev(%s) is busy", page_manager_get_page_prev_name(self));
        }
    }

    return ret;
}

/**
 * @brief Page switching animation execution end callback
 * @note  Invoked by LVGL when a page's switch animation finishes. Advances
 *        that page's state machine, clears its busy flag, then checks whether
 *        both the entering and exiting pages are done. When a pop has fully
 *        completed it refreshes the current animation type for the next move.
 * @param a Pointer to animation (its user-data is the PageBase_t*)
 * @retval None
 */
static void on_switch_anim_finish(lv_anim_t* a)
{
    PageBase_t* base = (PageBase_t*)lv_anim_get_user_data(a);
    PageManager_t* manager = base->manager;
    
    if (manager == NULL) return;

    PM_LOG_INFO("Page(%s) anim finish", base->name);

    page_manager_state_update(manager, base);
    base->priv.anim.is_busy = false;
    bool is_finished = page_manager_switch_req_check(manager);

    if (!manager->anim_state.is_entering && is_finished) {
        page_manager_switch_anim_type_update(manager, manager->page_current);
    }
}

/**
 * @brief Create page switching animation
 * @param self Pointer to page manager
 * @param base Pointer to the animated page
 * @retval None
 */
void page_manager_switch_anim_create(PageManager_t* self, PageBase_t* base)
{
    if (self == NULL || base == NULL) return;

    LoadAnimAttr_t anim_attr;
    if (!page_manager_get_current_load_anim_attr(self, &anim_attr)) {
        return;
    }

    lv_anim_t a;
    page_manager_anim_default_init(self, &a);
    lv_anim_set_user_data(&a, base);
    lv_anim_set_var(&a, base->root);
    lv_anim_set_ready_cb(&a, on_switch_anim_finish);
    lv_anim_set_exec_cb(&a, anim_attr.setter);

    int32_t start = 0;

    if (anim_attr.getter) {
        start = anim_attr.getter(base->root);
    }

    if (self->anim_state.is_entering) {
        if (base->priv.anim.is_enter) {
            lv_anim_set_values(&a, anim_attr.push.enter.start, anim_attr.push.enter.end);
        } else { /* Exit */
            lv_anim_set_values(&a, start, anim_attr.push.exit.end);
        }
    } else { /* Pop */
        if (base->priv.anim.is_enter) {
            lv_anim_set_values(&a, anim_attr.pop.enter.start, anim_attr.pop.enter.end);
        } else { /* Exit */
            lv_anim_set_values(&a, start, anim_attr.pop.exit.end);
        }
    }

    lv_anim_start(&a);
    base->priv.anim.is_busy = true;
}