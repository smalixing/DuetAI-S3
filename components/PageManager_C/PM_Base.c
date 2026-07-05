/*
 * MIT License
 * C-language port of PageManager - base / lifecycle / pool / stack.
 */
#include "PageManager.h"
#include "PM_Log.h"
#include <stdlib.h>
#include <string.h>

#define PM_EMPTY_PAGE_NAME "EMPTY_PAGE"

/* ------------------------------------------------------------------ */
/* PageArray_t helpers                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief  Initialise an empty page array.
 * @note   Sets all fields to zero; no memory is allocated until the first
 *         insertion. Must be called before any other page_array_* helper.
 * @param  arr  Pointer to the array descriptor to initialise.
 * @retval None
 */
static void page_array_init(PageArray_t* arr)
{
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

/**
 * @brief  Release the storage owned by a page array.
 * @note   Only the backing pointer buffer is freed; the PageBase_t objects
 *         the array points to are NOT destroyed (they are owned by the page
 *         pool / factory). The descriptor is reset to the empty state so it
 *         can be reused safely.
 * @param  arr  Pointer to the array descriptor to release.
 * @retval None
 */
static void page_array_deinit(PageArray_t* arr)
{
    if (arr->data) {
        lv_mem_free(arr->data);
    }
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

/**
 * @brief  Ensure the array can hold at least @p need elements.
 * @note   Growth strategy mirrors std::vector: capacity doubles (starting at
 *         4) until it covers @p need. On growth a new buffer is allocated via
 *         lv_mem_alloc(), existing elements are copied over, and the old
 *         buffer is freed. A no-op when the current capacity already suffices.
 * @param  arr   Pointer to the array descriptor.
 * @param  need  Minimum number of elements the array must accommodate.
 * @retval true  Capacity is now >= @p need.
 * @retval false Allocation failed (out of memory); array left unchanged.
 */
static bool page_array_reserve(PageArray_t* arr, uint32_t need)
{
    if (need <= arr->capacity) {
        return true;
    }
    uint32_t new_cap = arr->capacity ? arr->capacity * 2 : 4;
    while (new_cap < need) new_cap *= 2;

    PageBase_t** new_buf = (PageBase_t**)lv_mem_alloc(new_cap * sizeof(PageBase_t*));
    if (new_buf == NULL) {
        PM_LOG_ERROR("page_array_reserve: oom");
        return false;
    }
    if (arr->data) {
        memcpy(new_buf, arr->data, arr->size * sizeof(PageBase_t*));
        lv_mem_free(arr->data);
    }
    arr->data = new_buf;
    arr->capacity = new_cap;
    return true;
}

/**
 * @brief  Append a page pointer to the end of the array.
 * @note   Reserves room for one more element first, then stores @p p and
 *         increments the size. This is the array's "push_back" / stack-push.
 *         Exposed (non-static) because PM_Router.c pushes onto the stack
 *         while the array implementation lives here.
 * @param  arr  Pointer to the array descriptor.
 * @param  p    Page pointer to append.
 * @retval true  The page was stored.
 * @retval false Allocation failed; array left unchanged.
 */
bool page_array_push(PageArray_t* arr, PageBase_t* p)
{
    if (!page_array_reserve(arr, arr->size + 1)) return false;
    arr->data[arr->size++] = p;
    return true;
}

/**
 * @brief  Remove the last element of the array (stack pop).
 * @note   Only shrinks the logical size; capacity and the freed slot's
 *         pointer are left untouched. Safe to call on an empty array (no-op).
 * @param  arr  Pointer to the array descriptor.
 * @retval None
 */
static void page_array_pop(PageArray_t* arr)
{
    if (arr->size > 0) arr->size--;
}

/**
 * @brief  Peek at the last element of the array (stack top).
 * @param  arr  Pointer to the array descriptor.
 * @retval Pointer to the top page, or NULL if the array is empty.
 */
static PageBase_t* page_array_top(const PageArray_t* arr)
{
    return arr->size ? arr->data[arr->size - 1] : NULL;
}

/**
 * @brief  Remove the first occurrence of a page pointer from the array.
 * @note   Performs a linear search for @p p; on a match the trailing elements
 *         are shifted down by one to keep the array contiguous, then the size
 *         is decremented. Used to drop a page from the pool on unregister.
 * @param  arr  Pointer to the array descriptor.
 * @param  p    Page pointer to remove.
 * @retval true  The pointer was found and removed.
 * @retval false The pointer was not present in the array.
 */
static bool page_array_erase(PageArray_t* arr, PageBase_t* p)
{
    for (uint32_t i = 0; i < arr->size; i++) {
        if (arr->data[i] == p) {
            for (uint32_t j = i + 1; j < arr->size; j++) {
                arr->data[j - 1] = arr->data[j];
            }
            arr->size--;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief  Construct (initialise) a page manager instance.
 * @note   Zeroes the whole structure, stores the factory, initialises the
 *         empty pool and stack arrays, and applies the default global load
 *         animation (OVER_LEFT, 500 ms, ease-out). @p factory may be NULL if
 *         you only ever call page_manager_register() (i.e. you never use
 *         page_manager_install()).
 * @param  self     Pointer to the page manager storage to initialise.
 * @param  factory  Page factory used by Install(), or NULL.
 * @retval None
 */
void page_manager_init(PageManager_t* self, PageFactory_t* factory)
{
    if (self == NULL) return;
    memset(self, 0, sizeof(*self));
    self->factory = factory;
    page_array_init(&self->pool);
    page_array_init(&self->stack);

    page_manager_set_global_load_anim_type(self, LOAD_ANIM_OVER_LEFT, 500, lv_anim_path_ease_out);
}

/**
 * @brief  Destruct a page manager instance.
 * @note   Clears the page stack (running each page's full unload life cycle)
 *         and frees the pool/stack backing buffers. Does not free the
 *         PageManager_t struct itself, which the caller owns.
 * @param  self  Pointer to the page manager to tear down.
 * @retval None
 */
void page_manager_deinit(PageManager_t* self)
{
    if (self == NULL) return;
    page_manager_set_stack_clear(self, false);
    page_array_deinit(&self->stack);
    page_array_deinit(&self->pool);
}

/* ------------------------------------------------------------------ */
/* Pool / stack lookup                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief  Find an installed page in the page pool by name.
 * @note   The pool holds every page that has been installed/registered,
 *         whether or not it is currently on the stack. Lookup is a linear
 *         scan comparing names with strcmp().
 * @param  self  Pointer to the page manager.
 * @param  name  Application name of the page to find.
 * @retval Pointer to the matching page, or NULL if not found / bad args.
 */
PageBase_t* page_manager_find_page_in_pool(PageManager_t* self, const char* name)
{
    if (self == NULL || name == NULL) return NULL;
    for (uint32_t i = 0; i < self->pool.size; i++) {
        PageBase_t* p = self->pool.data[i];
        if (p && p->name && strcmp(name, p->name) == 0) {
            return p;
        }
    }
    return NULL;
}

/**
 * @brief  Find a page that is currently on the navigation stack by name.
 * @note   Used to detect duplicate pushes and to prevent unregistering a
 *         page that is still live on the stack. Linear scan over the stack.
 * @param  self  Pointer to the page manager.
 * @param  name  Application name of the page to find.
 * @retval Pointer to the matching page, or NULL if not on the stack / bad args.
 */
PageBase_t* page_manager_find_page_in_stack(PageManager_t* self, const char* name)
{
    if (self == NULL || name == NULL) return NULL;
    for (uint32_t i = 0; i < self->stack.size; i++) {
        PageBase_t* p = self->stack.data[i];
        if (p && p->name && strcmp(name, p->name) == 0) {
            return p;
        }
    }
    return NULL;
}

/**
 * @brief  Get the page on top of the navigation stack (the current page).
 * @param  self  Pointer to the page manager.
 * @retval Pointer to the top page, or NULL if the stack is empty / bad arg.
 */
PageBase_t* page_manager_get_stack_top(PageManager_t* self)
{
    return self ? page_array_top(&self->stack) : NULL;
}

/**
 * @brief  Get the page immediately below the top of the stack.
 * @note   This is the page that will become current after a Pop(), and the
 *         one revealed underneath during an "over" drag/animation. Requires
 *         at least two pages on the stack.
 * @param  self  Pointer to the page manager.
 * @retval Pointer to the second-from-top page, or NULL if fewer than two
 *         pages are on the stack / bad arg.
 */
PageBase_t* page_manager_get_stack_top_after(PageManager_t* self)
{
    if (self == NULL || self->stack.size < 2) return NULL;
    return self->stack.data[self->stack.size - 2];
}

/* ------------------------------------------------------------------ */
/* Install / Uninstall / Register / Unregister                         */
/* ------------------------------------------------------------------ */

/**
 * @brief  Install a page: create it via the factory and register it in the pool.
 * @note   Requires a factory (set at Init). When @p app_name is NULL the
 *         class name is used as the application name. The application name
 *         must be unique within the pool. After creation the page's public
 *         fields and private state are reset (the vtable is preserved), the
 *         page is registered, and its on_custom_attr_config() hook is invoked.
 * @param  self        Pointer to the page manager.
 * @param  class_name  Class name passed to the factory to create the page.
 * @param  app_name    Unique application/instance name (NULL => use class_name).
 * @retval true  The page was created and registered.
 * @retval false No factory, duplicate name, or factory could not create it.
 */
bool page_manager_install(PageManager_t* self, const char* class_name, const char* app_name)
{
    if (self == NULL) return false;

    if (self->factory == NULL) {
        PM_LOG_ERROR("Factory was not registered, can't install page");
        return false;
    }

    if (app_name == NULL) {
        PM_LOG_WARN("app_name has not set");
        app_name = class_name;
    }

    if (page_manager_find_page_in_pool(self, app_name) != NULL) {
        PM_LOG_ERROR("Page(%s) was registered", app_name);
        return false;
    }

    PageBase_t* base = page_factory_create_page(self->factory, class_name);
    if (base == NULL) {
        PM_LOG_ERROR("Factory has not %s", class_name);
        return false;
    }

    /* Reset public/private state but keep vtable. */
    base->root     = NULL;
    base->manager  = NULL;
    base->name     = NULL;
    base->id       = 0;
    base->user_data = NULL;
    memset(&base->priv, 0, sizeof(base->priv));

    PM_LOG_INFO("Install Page[class = %s, name = %s]", class_name, app_name);
    bool retval = page_manager_register(self, base, app_name);

    if (base->vtable && base->vtable->on_custom_attr_config) {
        base->vtable->on_custom_attr_config(base);
    }

    return retval;
}

/**
 * @brief  Uninstall a page: unregister it from the pool and destroy it.
 * @note   Fails if the page is not in the pool or is still on the stack
 *         (Unregister rejects in-stack pages). If the page still holds a
 *         cache it is force-unloaded (state driven to UNLOAD) before the
 *         page's on_destroy() hook is called so the implementation can free
 *         its own memory.
 * @param  self      Pointer to the page manager.
 * @param  app_name  Application name of the page to uninstall.
 * @retval true  The page was unregistered and destroyed.
 * @retval false Page not found, or it could not be unregistered.
 */
bool page_manager_uninstall(PageManager_t* self, const char* app_name)
{
    if (self == NULL) return false;
    PM_LOG_INFO("Page(%s) uninstall...", app_name);

    PageBase_t* base = page_manager_find_page_in_pool(self, app_name);
    if (base == NULL) {
        PM_LOG_ERROR("Page(%s) was not found", app_name);
        return false;
    }

    if (!page_manager_unregister(self, app_name)) {
        PM_LOG_ERROR("Page(%s) unregister failed", app_name);
        return false;
    }

    if (base->priv.is_cached) {
        PM_LOG_WARN("Page(%s) has cached, unloading...", app_name);
        base->priv.state = PAGE_STATE_UNLOAD;
        page_manager_state_update(self, base);
    } else {
        PM_LOG_INFO("Page(%s) has not cache", app_name);
    }

    if (base->vtable && base->vtable->on_destroy) {
        base->vtable->on_destroy(base);
    }

    PM_LOG_INFO("Uninstall OK");
    return true;
}

/**
 * @brief  Register an already-constructed page into the page pool.
 * @note   Lower-level alternative to Install() for pages you create yourself
 *         (no factory needed). Rejects duplicate names. On success the page's
 *         owning manager and name are recorded and it is appended to the pool.
 * @param  self  Pointer to the page manager.
 * @param  base  Pointer to the caller-owned, initialised page.
 * @param  name  Unique application name to register the page under.
 * @retval true  The page was added to the pool.
 * @retval false Bad args, duplicate name, or allocation failed.
 */
bool page_manager_register(PageManager_t* self, PageBase_t* base, const char* name)
{
    if (self == NULL || base == NULL || name == NULL) return false;

    if (page_manager_find_page_in_pool(self, name) != NULL) {
        PM_LOG_ERROR("Page(%s) was multi registered", name);
        return false;
    }

    base->manager = self;
    base->name    = name;

    if (!page_array_push(&self->pool, base)) {
        return false;
    }
    return true;
}

/**
 * @brief  Remove a page from the page pool.
 * @note   Refuses to unregister a page that is still on the navigation stack
 *         (it must be popped first). Removes the pool entry but does not
 *         destroy the page object itself; Uninstall() handles destruction.
 * @param  self  Pointer to the page manager.
 * @param  name  Application name of the page to unregister.
 * @retval true  The page was removed from the pool.
 * @retval false Bad args, page in stack, or page not found in pool.
 */
bool page_manager_unregister(PageManager_t* self, const char* name)
{
    if (self == NULL || name == NULL) return false;
    PM_LOG_INFO("Page(%s) unregister...", name);

    PageBase_t* base = page_manager_find_page_in_stack(self, name);
    if (base != NULL) {
        PM_LOG_ERROR("Page(%s) was in stack", name);
        return false;
    }

    base = page_manager_find_page_in_pool(self, name);
    if (base == NULL) {
        PM_LOG_ERROR("Page(%s) was not found", name);
        return false;
    }

    if (!page_array_erase(&self->pool, base)) {
        PM_LOG_ERROR("Page(%s) was not found in PagePool", name);
        return false;
    }

    PM_LOG_INFO("Unregister OK");
    return true;
}

/* ------------------------------------------------------------------ */
/* Stack utilities                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief  Pop and unload every page on the stack, ending their life cycles.
 * @note   Iterates from the top down, force-unloading each page (no
 *         animation). When @p keep_bottom is true the bottom-most page is
 *         left on the stack and recorded as the previous page (used by
 *         BackHome to return to the root page); otherwise the stack is fully
 *         emptied. _PagePrev is updated as the bottom is reached.
 * @param  self         Pointer to the page manager.
 * @param  keep_bottom  true to keep the stack's bottom (home) page.
 * @retval None
 */
void page_manager_set_stack_clear(PageManager_t* self, bool keep_bottom)
{
    if (self == NULL) return;

    while (1) {
        PageBase_t* top = page_manager_get_stack_top(self);
        if (top == NULL) {
            PM_LOG_INFO("Page stack is empty, breaking...");
            break;
        }

        PageBase_t* top_after = page_manager_get_stack_top_after(self);

        if (top_after == NULL) {
            if (keep_bottom) {
                self->page_prev = top;
                PM_LOG_INFO("Keep page stack bottom(%s), breaking...", top->name);
                break;
            } else {
                self->page_prev = NULL;
            }
        }

        page_manager_fource_unload(self, top);
        page_array_pop(&self->stack);
    }
    PM_LOG_INFO("Stack clear done");
}

/**
 * @brief  Get the name of the previous page.
 * @param  self  Pointer to the page manager.
 * @retval The previous page's name, or PM_EMPTY_PAGE_NAME if there is none.
 */
const char* page_manager_get_page_prev_name(PageManager_t* self)
{
    if (self == NULL) return PM_EMPTY_PAGE_NAME;
    return self->page_prev ? self->page_prev->name : PM_EMPTY_PAGE_NAME;
}

/**
 * @brief  Get the name of the current (top) page.
 * @param  self  Pointer to the page manager.
 * @retval The current page's name, or PM_EMPTY_PAGE_NAME if there is none.
 */
const char* page_manager_get_page_current_name(PageManager_t* self)
{
    if (self == NULL) return PM_EMPTY_PAGE_NAME;
    return self->page_current ? self->page_current->name : PM_EMPTY_PAGE_NAME;
}

/**
 * @brief  Check whether a named page currently exists on the stack.
 * @param  self  Pointer to the page manager.
 * @param  name  Application name of the page to test.
 * @retval true  The page is on the navigation stack.
 * @retval false The page is not on the stack (or bad args).
 */
bool page_manager_check_pages_exist(PageManager_t* self, const char* name)
{
    return page_manager_find_page_in_stack(self, name) != NULL;
}

/**
 * @brief  Set the default LVGL style applied to every page's root object.
 * @note   The style is added to each page root in the LOAD state. Pass NULL
 *         to disable. The style object is borrowed, not copied; it must stay
 *         alive for the lifetime of the pages that use it.
 * @param  self   Pointer to the page manager.
 * @param  style  Pointer to the LVGL style, or NULL to clear.
 * @retval None
 */
void page_manager_set_root_default_style(PageManager_t* self, lv_style_t* style)
{
    if (self) self->root_default_style = style;
}