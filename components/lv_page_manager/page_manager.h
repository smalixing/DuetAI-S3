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

#include "page_base.h"
#include "page_factory.h"

/* ------------------------------------------------------------------ */
/* Enumerations                                                        */
/* ------------------------------------------------------------------ */

/* Page switching animation type */
typedef enum {
    /* Default (global) animation type */
    PM_LOAD_ANIM_GLOBAL = 0,

    /* New page overwrites old page */
    PM_LOAD_ANIM_OVER_LEFT,
    PM_LOAD_ANIM_OVER_RIGHT,
    PM_LOAD_ANIM_OVER_TOP,
    PM_LOAD_ANIM_OVER_BOTTOM,

    /* New page pushes old page */
    PM_LOAD_ANIM_MOVE_LEFT,
    PM_LOAD_ANIM_MOVE_RIGHT,
    PM_LOAD_ANIM_MOVE_TOP,
    PM_LOAD_ANIM_MOVE_BOTTOM,

    /* The new interface fades in, the old page fades out */
    PM_LOAD_ANIM_FADE_ON,

    /* No animation */
    PM_LOAD_ANIM_NONE,

    _PM_LOAD_ANIM_LAST = PM_LOAD_ANIM_NONE
} pm_load_anim_t;

/* Page dragging direction */
typedef enum {
    PM_ROOT_DRAG_DIR_NONE,
    PM_ROOT_DRAG_DIR_HOR,
    PM_ROOT_DRAG_DIR_VER
} pm_root_drag_dir_t;

/* ------------------------------------------------------------------ */
/* Animation descriptors                                               */
/* ------------------------------------------------------------------ */

typedef void   (*pm_anim_setter_t)(void*, int32_t);
typedef int32_t(*pm_anim_getter_t)(void*);

typedef struct {
    struct { int32_t start; int32_t end; } enter; /* enter side */
    struct { int32_t start; int32_t end; } exit;  /* exit side  */
} pm_anim_value_t;

typedef struct {
    pm_anim_setter_t    setter;
    pm_anim_getter_t    getter;
    pm_root_drag_dir_t  drag_dir;
    pm_anim_value_t     push;
    pm_anim_value_t     pop;
} pm_load_anim_attr_t;

/* ------------------------------------------------------------------ */
/* Internal dynamic array of pm_page_base_t pointers                       */
/* ------------------------------------------------------------------ */

typedef struct {
    pm_page_base_t**  data;
    uint32_t          size;
    uint32_t          capacity;
} pm_page_array_t;

/* Append a page to the array (grows on demand). Returns false on OOM.
 * Exposed because the router (pm_router.c) pushes onto the stack while the
 * array implementation lives in pm_base.c. */
bool page_array_push(pm_page_array_t* arr, pm_page_base_t* p);

/* ------------------------------------------------------------------ */
/* Page manager                                                        */
/* ------------------------------------------------------------------ */

struct pm_manager {
    /* Page factory */
    pm_factory_t* factory;

    /* Page pool (all installed pages) */
    pm_page_array_t pool;

/* Page stack (top is data[size-1]) */
    pm_page_array_t stack;

    /* Previous page */
    pm_page_base_t* page_prev;

    /* The current page */
    pm_page_base_t* page_current;

    /* Page animation status */
    struct {
        bool           is_switch_req; /* has switch request */
        bool           is_busy;      /* is switching */
        bool           is_entering;  /* is in entering action */

        pm_page_anim_attr_t current;     /* current animation properties */
        pm_page_anim_attr_t global;      /* global animation properties */
    } anim_state;

    /* Root default style (optional) */
    lv_style_t* root_default_style;
};

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/* Construct / destruct a PageManager.  `factory` may be NULL if you only
 * intend to call page_manager_register() (i.e. you don't use Install). */
void page_manager_init(pm_manager_t* self, pm_factory_t* factory);
void page_manager_deinit(pm_manager_t* self);

/* ------------------------------------------------------------------ */
/* Loader                                                              */
/* ------------------------------------------------------------------ */

/* Create a page via the factory and register it in the pool (unique app_name;
 * app_name==NULL uses class_name). Returns false on failure. */
bool page_manager_install(pm_manager_t* self, const char* class_name, const char* app_name);
/* Unregister and destroy an installed page (must not be on the stack). */
bool page_manager_uninstall(pm_manager_t* self, const char* app_name);
/* Register a caller-constructed page into the pool under a unique name. */
bool page_manager_register(pm_manager_t* self, pm_page_base_t* base, const char* name);
/* Remove a page from the pool (must not be on the stack). */
bool page_manager_unregister(pm_manager_t* self, const char* name);

/* ------------------------------------------------------------------ */
/* Router                                                              */
/* ------------------------------------------------------------------ */

/* Replace the top page with `name`, passing optional `stash` parameters. */
bool        page_manager_replace(pm_manager_t* self, const char* name, const pm_page_stash_t* stash);
/* Push `name` onto the stack (old page kept below), passing optional `stash`. */
bool        page_manager_push(pm_manager_t* self, const char* name, const pm_page_stash_t* stash);
/* Pop the top page and switch back to the one beneath it. */
bool        page_manager_pop(pm_manager_t* self);
/* Pop the top page off the stack without animating/showing the page below. */
bool        page_manager_pop_unshow(pm_manager_t* self);
/* Clear the stack down to the bottom (home) page and show it. */
bool        page_manager_back_home(pm_manager_t* self);
/* Clear the stack down to the bottom page without showing it. */
bool        page_manager_back_home_unshow(pm_manager_t* self);
/* Name of the previous page, or PM_EMPTY_PAGE_NAME if none. */
const char* page_manager_get_page_prev_name(pm_manager_t* self);
/* Name of the current (top) page, or PM_EMPTY_PAGE_NAME if none. */
const char* page_manager_get_page_current_name(pm_manager_t* self);
/* True if a page with `name` is currently on the stack. */
bool        page_manager_check_pages_exist(pm_manager_t* self, const char* name);

/* ------------------------------------------------------------------ */
/* global animation                                                    */
/* ------------------------------------------------------------------ */

/* Set the default load animation type, duration (ms) and easing path used by
 * pages that do not define their own. */
void page_manager_set_global_load_anim_type(
    pm_manager_t*     self,
    pm_load_anim_t    anim,
    uint16_t          time,
    lv_anim_path_cb_t path);

/* Set (or clear with NULL) the LVGL style applied to every page root. */
void page_manager_set_root_default_style(pm_manager_t* self, lv_style_t* style);

/* ------------------------------------------------------------------ */
/* Internals (exposed because they are split across multiple .c files) */
/* ------------------------------------------------------------------ */

/* Page pool / stack helpers */
pm_page_base_t* page_manager_find_page_in_pool(pm_manager_t* self, const char* name);
pm_page_base_t* page_manager_find_page_in_stack(pm_manager_t* self, const char* name);
pm_page_base_t* page_manager_get_stack_top(pm_manager_t* self);
pm_page_base_t* page_manager_get_stack_top_after(pm_manager_t* self);
void        page_manager_set_stack_clear(pm_manager_t* self, bool keep_bottom);
bool        page_manager_fource_unload(pm_manager_t* self, pm_page_base_t* base);

/* Switching */
bool page_manager_switch_to(pm_manager_t* self, pm_page_base_t* base, bool is_enter_act, const pm_page_stash_t* stash);
bool page_manager_switch_req_check(pm_manager_t* self);
bool page_manager_switch_anim_state_check(pm_manager_t* self);
void page_manager_switch_anim_create(pm_manager_t* self, pm_page_base_t* base);
void page_manager_switch_anim_type_update(pm_manager_t* self, pm_page_base_t* base);

/* Animation */
bool        page_manager_get_load_anim_attr(uint8_t anim, pm_load_anim_attr_t* attr);
pm_load_anim_t  page_manager_get_current_load_anim_type(pm_manager_t* self);
bool        page_manager_get_current_load_anim_attr(pm_manager_t* self, pm_load_anim_attr_t* attr);
bool        page_manager_get_is_over_anim(uint8_t anim);
bool        page_manager_get_is_move_anim(uint8_t anim);
void        page_manager_anim_default_init(pm_manager_t* self, lv_anim_t* a);

/* state machine */
pm_page_state_t page_manager_state_load_execute(pm_manager_t* self, pm_page_base_t* base);
pm_page_state_t page_manager_state_will_appear_execute(pm_manager_t* self, pm_page_base_t* base);
pm_page_state_t page_manager_state_did_appear_execute(pm_manager_t* self, pm_page_base_t* base);
pm_page_state_t page_manager_state_will_disappear_execute(pm_manager_t* self, pm_page_base_t* base);
pm_page_state_t page_manager_state_did_disappear_execute(pm_manager_t* self, pm_page_base_t* base);
pm_page_state_t page_manager_state_unload_execute(pm_manager_t* self, pm_page_base_t* base);
void        page_manager_state_update(pm_manager_t* self, pm_page_base_t* base);

/* Drag */
void page_manager_root_enable_drag(pm_manager_t* self, lv_obj_t* root);

/* Convenience invocation of optional vtable methods. */
#define PM_CALL_VFUNC(base, fn) \
    do { if ((base) && (base)->vtable && (base)->vtable->fn) (base)->vtable->fn(base); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* !__PAGE_MANAGER_H */