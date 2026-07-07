/*
 * MIT License
 * C-language port of PageBase implementation.
 */
#include "page_base.h"
#include "pm_log.h"

/**
 * @brief  Initialise a page base structure and bind its vtable.
 * @note   Must be called by a concrete page's constructor before the page is
 *         handed to page_manager_register()/Install(). Zeroes all public and
 *         private fields, then stores the vtable of life-cycle callbacks.
 * @param  self    Pointer to the (embedded) pm_page_base_t to initialise.
 * @param  vtable  Pointer to the page's callback table (may contain NULLs).
 * @retval None
 */
void page_base_init(pm_page_base_t* self, const pm_page_vtable_t* vtable)
{
    if (self == NULL) {
        return;
    }
    memset(self, 0, sizeof(*self));
    self->vtable = vtable;
}

/**
 * @brief  Enable or disable manual cache control for the page.
 * @note   Turning this on first disables automatic cache management (so the
 *         page won't be cached/unloaded automatically), then records the
 *         requested cache state, which is applied when the page next loads.
 * @param  self  Pointer to the page.
 * @param  en    true to keep the page cached under manual control.
 * @retval None
 */
void page_base_set_custom_cache_enable(pm_page_base_t* self, bool en)
{
    if (self == NULL) return;
    PM_LOG_INFO("Page(%s) set_custom_cache_enable = %d", self->name, en);
    page_base_set_custom_auto_cache_enable(self, false);
    self->priv.req_enable_cache = en;
}

/**
 * @brief  Enable or disable automatic cache management for the page.
 * @note   When automatic caching is on (the default) the manager decides
 *         whether to keep the page loaded across hide/show. Stored as the
 *         inverse "disable auto cache" request, applied when the page loads.
 * @param  self  Pointer to the page.
 * @param  en    true to let the manager auto-manage the cache.
 * @retval None
 */
void page_base_set_custom_auto_cache_enable(pm_page_base_t* self, bool en)
{
    if (self == NULL) return;
    PM_LOG_INFO("Page(%s) set_custom_auto_cache_enable = %d", self->name, en);
    self->priv.req_disable_auto_cache = !en;
}

/**
 * @brief  Override the page's load/switch animation.
 * @note   Sets a page-specific animation type, duration and easing path that
 *         take precedence over the manager's global settings (unless the type
 *         is PM_LOAD_ANIM_GLOBAL, which means "inherit global").
 * @param  self       Pointer to the page.
 * @param  anim_type  Animation type (pm_load_anim_t value).
 * @param  time       Animation duration in milliseconds.
 * @param  path       LVGL easing/path callback for the animation curve.
 * @retval None
 */
void page_base_set_custom_load_anim_type(
    pm_page_base_t*   self,
    uint8_t           anim_type,
    uint16_t          time,
    lv_anim_path_cb_t path)
{
    if (self == NULL) return;
    self->priv.anim.attr.type = anim_type;
    self->priv.anim.attr.time = time;
    self->priv.anim.attr.path = path;
}

/**
 * @brief  Retrieve and consume the page's stashed parameter data.
 * @note   Copies the stash into @p ptr only if a stash exists and its size
 *         exactly matches @p size, then frees the stash buffer so it is
 *         consumed once. A size mismatch or absent stash is reported and
 *         leaves @p ptr untouched.
 * @param  self  Pointer to the page holding the stash.
 * @param  ptr   Destination buffer to receive the stashed data.
 * @param  size  Expected size of the stashed data, in bytes.
 * @retval true  Data was copied out and the stash was freed.
 * @retval false No stash, size mismatch, or bad args.
 */
bool page_base_stash_pop(pm_page_base_t* self, void* ptr, uint32_t size)
{
    if (self == NULL || ptr == NULL) {
        return false;
    }

    if (self->priv.stash.ptr == NULL) {
        PM_LOG_WARN("No stash found");
        return false;
    }

    if (self->priv.stash.size != size) {
        PM_LOG_WARN(
           "stash[0x%p](%d) does not match the size(%d)",
            self->priv.stash.ptr,
            (int)self->priv.stash.size,
            (int)size);
        return false;
    }

    memcpy(ptr, self->priv.stash.ptr, self->priv.stash.size);
    lv_mem_free(self->priv.stash.ptr);
    self->priv.stash.ptr  = NULL;
    self->priv.stash.size = 0;
    return true;
}