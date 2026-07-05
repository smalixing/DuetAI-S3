/*
 * MIT License
 * C-language port of PageBase.
 *
 * Design notes:
 *   - In the C++ original PageBase is a class with virtual methods.
 *   - In this C port, the "virtual table" is replaced by a struct of function
 *     pointers (PageVTable_t). Each concrete page provides its own vtable.
 *   - Each page is described by a PageBase_t struct that embeds the vtable
 *     pointer plus the private/public state that the page manager needs.
 */
#ifndef __PAGE_BASE_H
#define __PAGE_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Generate stash area data */
#define PAGE_STASH_MAKE(data) { &(data), sizeof(data) }

/* Get the data in the stash area */
#define PAGE_STASH_POP(page, data) page_base_stash_pop((page), &(data), sizeof(data))

#define PAGE_ANIM_TIME_DEFAULT 500 /* [ms] */
#define PAGE_ANIM_PATH_DEFAULT lv_anim_path_ease_out

/* Forward declarations */
typedef struct PageManager  PageManager_t;
typedef struct PageBase     PageBase_t;

/* Page state */
typedef enum {
    PAGE_STATE_IDLE,
    PAGE_STATE_LOAD,
    PAGE_STATE_WILL_APPEAR,
    PAGE_STATE_DID_APPEAR,
    PAGE_STATE_ACTIVITY,
    PAGE_STATE_WILL_DISAPPEAR,
    PAGE_STATE_DID_DISAPPEAR,
    PAGE_STATE_UNLOAD,
    _PAGE_STATE_LAST
} PageState_t;

/* stash data area */
typedef struct {
    void*    ptr;
    uint32_t size;
} PageStash_t;

/* Page switching animation properties */
typedef struct {
    uint8_t           type;
    uint16_t          time;
    lv_anim_path_cb_t path;
} PageAnimAttr_t;

/* "Virtual" methods.  Any NULL entry is treated as a no-op. */
typedef struct {
    /* Synchronize user-defined attribute configuration */
    void (*on_custom_attr_config)(PageBase_t* self);
    /* Page load start / end */
    void (*on_view_load)(PageBase_t* self);
    void (*on_view_did_load)(PageBase_t* self);
    /* Page appear animation start / end */
    void (*on_view_will_appear)(PageBase_t* self);
    void (*on_view_did_appear)(PageBase_t* self);
    /* Page disappear animation start / end */
    void (*on_view_will_disappear)(PageBase_t* self);
    void (*on_view_did_disappear)(PageBase_t* self);
    /* Page unload start / end */
    void (*on_view_unload)(PageBase_t* self);
    void (*on_view_did_unload)(PageBase_t* self);
    /* Destructor (called by Uninstall after the page is unloaded). */
    void (*on_destroy)(PageBase_t* self);
} PageVTable_t;

/* The base page descriptor. Concrete pages should embed it as the first
 * field of their own struct, so a pointer to that struct can be cast to
 * (PageBase_t*) and vice versa. */
struct PageBase {
    /* "vtable" */
    const PageVTable_t* vtable;

    /* Public fields, exposed to the page implementations. */
    lv_obj_t*       root;     /* UI root node */
    PageManager_t*  manager;  /* owning page manager */
    const char*     name;     /* page name */
    uint16_t        id;       /* page ID */
    void*           user_data; /* user data pointer */

    /* Private data; only the page manager should touch these. */
    struct {
        bool        req_enable_cache;      /* cache enable request */
        bool        req_disable_auto_cache; /* automatic cache management disable request */

        bool        is_disable_auto_cache;  /* whether auto cache is disabled */
        bool        is_cached;            /* cache enable */

        PageStash_t stash;               /* stash area */
        PageState_t state;               /* page state */

        /* Animation state */
        struct {
            bool           is_enter; /* whether it is the entering party */
            bool           is_busy;  /* whether the animation is playing */
            PageAnimAttr_t attr;    /* animation properties */
        } anim;
    } priv;
};

/* ------------------------------------------------------------------ */
/* Public helpers                                                      */
/* ------------------------------------------------------------------ */

/* Initialise a PageBase struct.  Must be called by the concrete page's
 * constructor before handing the page to page_manager_register/Install. */
void page_base_init(PageBase_t* self, const PageVTable_t* vtable);

/* Set whether to manually manage the cache */
void page_base_set_custom_cache_enable(PageBase_t* self, bool en);

/* Set whether to enable automatic cache */
void page_base_set_custom_auto_cache_enable(PageBase_t* self, bool en);

/* Set custom animation properties */
void page_base_set_custom_load_anim_type(
    PageBase_t*       self,
    uint8_t           anim_type,
    uint16_t          time,
    lv_anim_path_cb_t path);

/* Pop the data from stash area. Returns true on success. */
bool page_base_stash_pop(PageBase_t* self, void* ptr, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* !__PAGE_BASE_H */