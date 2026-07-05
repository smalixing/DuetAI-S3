/*
 * MIT License
 * C-language port of PageManager.
 *
 * The C++ std::vector / std::stack containers are replaced by a single
 * dynamic array that doubles as both pool and stack (stack[0..stack_top-1]
 * mirrors the C++ std::stack, pool entries live in a separate dynamic
 * array).  All behaviour, naming and the state machine are kept as close
 * to the original as possible.
 */
#ifndef __PAGE_MANAGER_H
#define __PAGE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "PageBase.h"
#include "PageFactory.h"

/* ------------------------------------------------------------------ */
/* Enumerations                                                        */
/* ------------------------------------------------------------------ */

/* Page switching animation type */
typedef enum {
    /* Default (global) animation type */
    LOAD_ANIM_GLOBAL = 0,

    /* New page overwrites old page */
    LOAD_ANIM_OVER_LEFT,
    LOAD_ANIM_OVER_RIGHT,
    LOAD_ANIM_OVER_TOP,
    LOAD_ANIM_OVER_BOTTOM,

    /* New page pushes old page */
    LOAD_ANIM_MOVE_LEFT,
    LOAD_ANIM_MOVE_RIGHT,
    LOAD_ANIM_MOVE_TOP,
    LOAD_ANIM_MOVE_BOTTOM,

    /* The new interface fades in, the old page fades out */
    LOAD_ANIM_FADE_ON,

    /* No animation */
    LOAD_ANIM_NONE,

    _LOAD_ANIM_LAST = LOAD_ANIM_NONE
} LoadAnim_t;

/* Page dragging direction */
typedef enum {
    ROOT_DRAG_DIR_NONE,
    ROOT_DRAG_DIR_HOR,
    ROOT_DRAG_DIR_VER
} RootDragDir_t;

/* ------------------------------------------------------------------ */
/* Animation descriptors                                               */
/* ------------------------------------------------------------------ */

typedef void   (*pm_anim_setter_t)(void*, int32_t);
typedef int32_t(*pm_anim_getter_t)(void*);

typedef struct {
    struct { int32_t start; int32_t end; } enter; /* enter side */
    struct { int32_t start; int32_t end; } exit;  /* exit side  */
} AnimValue_t;

typedef struct {
    pm_anim_setter_t setter;
    pm_anim_getter_t getter;
    RootDragDir_t    drag_dir;
    AnimValue_t      push;
    AnimValue_t      pop;
} LoadAnimAttr_t;

/* ------------------------------------------------------------------ */
/* Internal dynamic array of PageBase_t pointers                       */
/* ------------------------------------------------------------------ */

typedef struct {
    PageBase_t** data;
    uint32_t     size;
    uint32_t     capacity;
} PageArray_t;

/* Append a page to the array (grows on demand). Returns false on OOM.
 * Exposed because the router (PM_Router.c) pushes onto the stack while the
 * array implementation lives in PM_Base.c. */
bool page_array_push(PageArray_t* arr, PageBase_t* p);

/* ------------------------------------------------------------------ */
/* Page manager                                                        */
/* ------------------------------------------------------------------ */

struct PageManager {
    /* Page factory */
    PageFactory_t* factory;

    /* Page pool (all installed pages) */
    PageArray_t pool;

/* Page stack (top is data[size-1]) */
    PageArray_t stack;

    /* Previous page */
    PageBase_t* page_prev;

    /* The current page */
    PageBase_t* page_current;

    /* Page animation status */
    struct {
        bool           is_switch_req; /* has switch request */
        bool           is_busy;      /* is switching */
        bool           is_entering;  /* is in entering action */

        PageAnimAttr_t current;     /* current animation properties */
        PageAnimAttr_t global;      /* global animation properties */
    } anim_state;

    /* Root default style (optional) */
    lv_style_t* root_default_style;
};

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/* Construct / destruct a PageManager.  `factory` may be NULL if you only
 * intend to call page_manager_register() (i.e. you don't use Install). */
void page_manager_init(PageManager_t* self, PageFactory_t* factory);
void page_manager_deinit(PageManager_t* self);

/* ------------------------------------------------------------------ */
/* Loader                                                              */
/* ------------------------------------------------------------------ */

/* Create a page via the factory and register it in the pool (unique app_name;
 * app_name==NULL uses class_name). Returns false on failure. */
bool page_manager_install(PageManager_t* self, const char* class_name, const char* app_name);
/* Unregister and destroy an installed page (must not be on the stack). */
bool page_manager_uninstall(PageManager_t* self, const char* app_name);
/* Register a caller-constructed page into the pool under a unique name. */
bool page_manager_register(PageManager_t* self, PageBase_t* base, const char* name);
/* Remove a page from the pool (must not be on the stack). */
bool page_manager_unregister(PageManager_t* self, const char* name);

/* ------------------------------------------------------------------ */
/* Router                                                              */
/* ------------------------------------------------------------------ */

/* Replace the top page with `name`, passing optional `stash` parameters. */
bool        page_manager_replace(PageManager_t* self, const char* name, const PageStash_t* stash);
/* Push `name` onto the stack (old page kept below), passing optional `stash`. */
bool        page_manager_push(PageManager_t* self, const char* name, const PageStash_t* stash);
/* Pop the top page and switch back to the one beneath it. */
bool        page_manager_pop(PageManager_t* self);
/* Pop the top page off the stack without animating/showing the page below. */
bool        page_manager_pop_unshow(PageManager_t* self);
/* Clear the stack down to the bottom (home) page and show it. */
bool        page_manager_back_home(PageManager_t* self);
/* Clear the stack down to the bottom page without showing it. */
bool        page_manager_back_home_unshow(PageManager_t* self);
/* Name of the previous page, or PM_EMPTY_PAGE_NAME if none. */
const char* page_manager_get_page_prev_name(PageManager_t* self);
/* Name of the current (top) page, or PM_EMPTY_PAGE_NAME if none. */
const char* page_manager_get_page_current_name(PageManager_t* self);
/* True if a page with `name` is currently on the stack. */
bool        page_manager_check_pages_exist(PageManager_t* self, const char* name);

/* ------------------------------------------------------------------ */
/* global animation                                                    */
/* ------------------------------------------------------------------ */

/* Set the default load animation type, duration (ms) and easing path used by
 * pages that do not define their own. */
void page_manager_set_global_load_anim_type(
    PageManager_t*    self,
    LoadAnim_t        anim,
    uint16_t          time,
    lv_anim_path_cb_t path);

/* Set (or clear with NULL) the LVGL style applied to every page root. */
void page_manager_set_root_default_style(PageManager_t* self, lv_style_t* style);

/* ------------------------------------------------------------------ */
/* Internals (exposed because they are split across multiple .c files) */
/* ------------------------------------------------------------------ */

/* Page pool / stack helpers */
PageBase_t* page_manager_find_page_in_pool(PageManager_t* self, const char* name);
PageBase_t* page_manager_find_page_in_stack(PageManager_t* self, const char* name);
PageBase_t* page_manager_get_stack_top(PageManager_t* self);
PageBase_t* page_manager_get_stack_top_after(PageManager_t* self);
void        page_manager_set_stack_clear(PageManager_t* self, bool keep_bottom);
bool        page_manager_fource_unload(PageManager_t* self, PageBase_t* base);

/* Switching */
bool page_manager_switch_to(PageManager_t* self, PageBase_t* base, bool is_enter_act, const PageStash_t* stash);
bool page_manager_switch_req_check(PageManager_t* self);
bool page_manager_switch_anim_state_check(PageManager_t* self);
void page_manager_switch_anim_create(PageManager_t* self, PageBase_t* base);
void page_manager_switch_anim_type_update(PageManager_t* self, PageBase_t* base);

/* Animation */
bool        page_manager_get_load_anim_attr(uint8_t anim, LoadAnimAttr_t* attr);
LoadAnim_t  page_manager_get_current_load_anim_type(PageManager_t* self);
bool        page_manager_get_current_load_anim_attr(PageManager_t* self, LoadAnimAttr_t* attr);
bool        page_manager_get_is_over_anim(uint8_t anim);
bool        page_manager_get_is_move_anim(uint8_t anim);
void        page_manager_anim_default_init(PageManager_t* self, lv_anim_t* a);

/* state machine */
PageState_t page_manager_state_load_execute(PageManager_t* self, PageBase_t* base);
PageState_t page_manager_state_will_appear_execute(PageManager_t* self, PageBase_t* base);
PageState_t page_manager_state_did_appear_execute(PageManager_t* self, PageBase_t* base);
PageState_t page_manager_state_will_disappear_execute(PageManager_t* self, PageBase_t* base);
PageState_t page_manager_state_did_disappear_execute(PageManager_t* self, PageBase_t* base);
PageState_t page_manager_state_unload_execute(PageManager_t* self, PageBase_t* base);
void        page_manager_state_update(PageManager_t* self, PageBase_t* base);

/* Drag */
void page_manager_root_enable_drag(PageManager_t* self, lv_obj_t* root);

/* Convenience invocation of optional vtable methods. */
#define PM_CALL_VFUNC(base, fn) \
    do { if ((base) && (base)->vtable && (base)->vtable->fn) (base)->vtable->fn(base); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* !__PAGE_MANAGER_H */