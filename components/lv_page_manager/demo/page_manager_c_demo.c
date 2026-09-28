/*
 * MIT License
 * PageManager_C usage demo - implementation.
 *
 * Demonstrates a tiny two-screen app built on the C PageManager:
 *
 *   HomePage
 *     +-- "Open Detail" button  -> push("DetailPage", stash={counter})
 *     +-- "Open Detail (Replace)" -> push then the detail can replace itself
 *
 *   DetailPage
 *     +-- shows the counter passed in via the stash
 *     +-- "Back" button         -> pop()       (return to Home)
 *     +-- "Re-open"             -> replace()    (swap itself for a fresh copy)
 *     +-- "Home" button         -> back_home()  (drop straight to the root)
 *
 * The whole flow is event driven: pressing a button calls into the manager,
 * which runs the page life-cycle callbacks and the slide animation.
 *
 * Key patterns shown:
 *   1. A concrete page = "struct { pm_page_base_t base; ...own fields...; }" so the
 *      page pointer can be cast to/from pm_page_base_t*.
 *   2. A static const pm_page_vtable_t per page type wires the life-cycle hooks.
 *   3. The factory allocates a concrete page by class name.
 *   4. Stash passes a small POD payload from caller to the new page.
 */
#include "page_manager_c_demo.h"
#include "../page_manager.h"
#include "../page_base.h"
#include "../page_factory.h"

/* ================================================================== */
/* Shared payload passed between pages via the stash                   */
/* ================================================================== */

/* Plain-old-data struct copied into the target page's stash on navigation. */
typedef struct {
    int counter; /* how many times Detail has been opened */
} detail_param_t;

/**
 * @brief  Shared destructor for demo pages.
 * @note   The manager calls this from page_manager_uninstall() AFTER the page
 *         has been unloaded; it does NOT free the page object itself, so the
 *         factory-allocated memory must be released here. Because pm_page_base_t
 *         is the first member of every concrete page struct, freeing @p base
 *         frees the whole home_page_t / detail_page_t allocation.
 * @param  base  Pointer to the page base to free.
 * @retval None
 */
static void demo_page_on_destroy(pm_page_base_t* base)
{
    lv_free(base);
}

/* ================================================================== */
/* HomePage                                                            */
/* ================================================================== */

/* Concrete page: embeds pm_page_base_t as its FIRST member (mandatory) so a
 * home_page_t* and a pm_page_base_t* are interchangeable via cast. */
typedef struct {
    pm_page_base_t base;           /* must be first */
    lv_obj_t*  btn_open;       /* "Open Detail" */
    lv_obj_t*  btn_open_again; /* "Open Detail (again)" */
    int        open_count;     /* bumped each time we open Detail */
} home_page_t;

/* The demo's single manager instance and a guard against double-start. */
static pm_manager_t s_manager;
static bool          s_started = false;

/**
 * @brief  HomePage button event handler.
 * @note   On a short click of either button, increments the open counter and
 *         pushes DetailPage, handing it the counter through the stash. Using
 *         PM_PAGE_STASH_MAKE() builds a {pointer,size} descriptor; the manager
 *         copies the bytes into the target page so the caller's local can go
 *         out of scope safely.
 * @param  e  LVGL event.
 * @retval None
 */
static void home_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_CLICKED) {
        return;
    }

    home_page_t* self = (home_page_t*)lv_event_get_user_data(e);

    self->open_count++;
    detail_param_t param = { .counter = self->open_count };
    pm_page_stash_t stash = PM_PAGE_STASH_MAKE(param);

    /* Push the detail page on top of Home, passing the counter. */
    page_manager_push(self->base.manager, "DetailPage", &stash);
}

/**
 * @brief  HomePage: build the UI (called once per load).
 * @note   Creates two buttons centered on the page root and attaches the
 *         click handler. The root object is provided by the manager in
 *         base->root before this hook runs.
 * @param  base  Pointer to the page base (cast to home_page_t* for own fields).
 * @retval None
 */
static void home_on_view_load(pm_page_base_t* base)
{
    home_page_t* self = (home_page_t*)base;

    lv_obj_set_style_bg_color(base->root, lv_color_hex(0x202840), LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(base->root);
    lv_label_set_text(title, "Home Page");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    self->btn_open = lv_btn_create(base->root);
    lv_obj_set_size(self->btn_open, 200, 60);
    lv_obj_align(self->btn_open, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(self->btn_open, home_event_cb, LV_EVENT_SHORT_CLICKED, self);
    lv_obj_t* l1 = lv_label_create(self->btn_open);
    lv_label_set_text(l1, "Open Detail");
    lv_obj_center(l1);

    self->btn_open_again = lv_btn_create(base->root);
    lv_obj_set_size(self->btn_open_again, 200, 60);
    lv_obj_align(self->btn_open_again, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(self->btn_open_again, home_event_cb, LV_EVENT_SHORT_CLICKED, self);
    lv_obj_t* l2 = lv_label_create(self->btn_open_again);
    lv_label_set_text(l2, "Open Detail (again)");
    lv_obj_center(l2);
}

/**
 * @brief  HomePage: configure custom attributes before first use.
 * @note   Keep Home cached (it is the root) and use a global animation. This
 *         hook runs once, right after install.
 * @param  base  Pointer to the page base.
 * @retval None
 */
static void home_on_custom_attr_config(pm_page_base_t* base)
{
    /* Let the manager auto-manage the cache (default). Nothing special here;
     * shown to illustrate where per-page config belongs. */
    (void)base;
}

/* HomePage life-cycle table. Unset slots are NULL and treated as no-ops. */
static const pm_page_vtable_t s_home_vtable = {
    .on_custom_attr_config = home_on_custom_attr_config,
    .on_view_load          = home_on_view_load,
    .on_destroy            = demo_page_on_destroy,
};

/* ================================================================== */
/* DetailPage                                                          */
/* ================================================================== */

typedef struct {
    pm_page_base_t base;        /* must be first */
    lv_obj_t*  lbl_counter; /* shows the stashed counter */
    lv_obj_t*  btn_back;    /* pop() */
    lv_obj_t*  btn_reopen;  /* replace() */
    lv_obj_t*  btn_home;    /* back_home() */
    int        counter;     /* value received via stash */
} detail_page_t;

/**
 * @brief  DetailPage button event handler.
 * @note   Dispatches on which button was clicked:
 *           - Back  : pop() returns to the page beneath (Home).
 *           - Re-open: replace() swaps this page for a fresh DetailPage,
 *                      forwarding an incremented counter via the stash.
 *           - Home  : back_home() clears the stack down to the root page.
 * @param  e  LVGL event.
 * @retval None
 */
static void detail_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_CLICKED) {
        return;
    }

    detail_page_t* self = (detail_page_t*)lv_event_get_user_data(e);
    lv_obj_t*     btn  = lv_event_get_current_target(e);

    if (btn == self->btn_back) {
        page_manager_pop(self->base.manager);
    } else if (btn == self->btn_reopen) {
        detail_param_t param = { .counter = self->counter + 1 };
        pm_page_stash_t   stash = PM_PAGE_STASH_MAKE(param);
        page_manager_replace(self->base.manager, "DetailPage", &stash);
    } else if (btn == self->btn_home) {
        page_manager_back_home(self->base.manager);
    }
}

/**
 * @brief  DetailPage: build the UI (called once per load).
 * @param  base  Pointer to the page base.
 * @retval None
 */
static void detail_on_view_load(pm_page_base_t* base)
{
    detail_page_t* self = (detail_page_t*)base;

    lv_obj_set_style_bg_color(base->root, lv_color_hex(0x283848), LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(base->root);
    lv_label_set_text(title, "Detail Page");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    self->lbl_counter = lv_label_create(base->root);
    lv_obj_align(self->lbl_counter, LV_ALIGN_TOP_MID, 0, 70);
    /* Text is filled in on_view_will_appear once the stash is read. */
    lv_label_set_text(self->lbl_counter, "");

    self->btn_back = lv_btn_create(base->root);
    lv_obj_set_size(self->btn_back, 180, 56);
    lv_obj_align(self->btn_back, LV_ALIGN_CENTER, 0, -60);
    lv_obj_add_event_cb(self->btn_back, detail_event_cb, LV_EVENT_SHORT_CLICKED, self);
    lv_obj_t* lb = lv_label_create(self->btn_back);
    lv_label_set_text(lb, "Back (Pop)");
    lv_obj_center(lb);

    self->btn_reopen = lv_btn_create(base->root);
    lv_obj_set_size(self->btn_reopen, 180, 56);
    lv_obj_align(self->btn_reopen, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_event_cb(self->btn_reopen, detail_event_cb, LV_EVENT_SHORT_CLICKED, self);
    lv_obj_t* lr = lv_label_create(self->btn_reopen);
    lv_label_set_text(lr, "Re-open (Replace)");
    lv_obj_center(lr);

    self->btn_home = lv_btn_create(base->root);
    lv_obj_set_size(self->btn_home, 180, 56);
    lv_obj_align(self->btn_home, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_event_cb(self->btn_home, detail_event_cb, LV_EVENT_SHORT_CLICKED, self);
    lv_obj_t* lh = lv_label_create(self->btn_home);
    lv_label_set_text(lh, "Home (BackHome)");
    lv_obj_center(lh);
}

/**
 * @brief  DetailPage: read the stashed parameter just before appearing.
 * @note   PM_PAGE_STASH_POP() copies the payload out of the stash (size-checked)
 *         and consumes it. We read here rather than in load so a cached page
 *         re-shown via replace() also refreshes its displayed value.
 * @param  base  Pointer to the page base.
 * @retval None
 */
static void detail_on_view_will_appear(pm_page_base_t* base)
{
    detail_page_t* self = (detail_page_t*)base;

    detail_param_t param = { .counter = 0 };
    if (PM_PAGE_STASH_POP(base, param)) {
        self->counter = param.counter;
    }

    lv_label_set_text_fmt(self->lbl_counter, "Opened #%d", self->counter);
}

/* DetailPage life-cycle table. */
static const pm_page_vtable_t s_detail_vtable = {
    .on_view_load         = detail_on_view_load,
    .on_view_will_appear  = detail_on_view_will_appear,
    .on_destroy           = demo_page_on_destroy,
};

/* ================================================================== */
/* Factory                                                             */
/* ================================================================== */

/**
 * @brief  Demo factory: construct a page by class name.
 * @note   Allocates the concrete page struct, initialises its embedded base
 *         with the matching vtable, and returns &page->base. The manager
 *         takes over ownership; the page frees itself in on_destroy (below).
 *         Returns NULL for unknown class names.
 * @param  factory     The factory instance (unused; no shared context needed).
 * @param  class_name  Class name requested by page_manager_install().
 * @retval Pointer to the new page base, or NULL on unknown name / OOM.
 */
static pm_page_base_t* demo_factory_create(pm_factory_t* factory, const char* class_name)
{
    (void)factory;

    if (strcmp(class_name, "HomePage") == 0) {
        home_page_t* p = (home_page_t*)lv_malloc(sizeof(home_page_t));
        if (!p) return NULL;
        page_base_init(&p->base, &s_home_vtable);
        return &p->base;
    }

    if (strcmp(class_name, "DetailPage") == 0) {
        detail_page_t* p = (detail_page_t*)lv_malloc(sizeof(detail_page_t));
        if (!p) return NULL;
        page_base_init(&p->base, &s_detail_vtable);
        return &p->base;
    }

    return NULL;
}

/* The factory is a struct of a create callback (+ optional user_ctx). */
static pm_factory_t s_factory = {
    .create   = demo_factory_create,
    .user_ctx = NULL,
};

/* ================================================================== */
/* Public entry points                                                 */
/* ================================================================== */

void page_manager_c_demo_start(void)
{
    if (s_started) {
        return;
    }

    /* 1. Bring up the manager with our factory. */
    page_manager_init(&s_manager, &s_factory);

    /* 2. Pick a default slide animation for all page switches. */
    page_manager_set_global_load_anim_type(
        &s_manager, PM_LOAD_ANIM_MOVE_LEFT, 300, lv_anim_path_ease_out);

    /* 3. Install the pages (class name == app name here for simplicity). */
    page_manager_install(&s_manager, "HomePage",   "HomePage");
    page_manager_install(&s_manager, "DetailPage", "DetailPage");

    /* 4. Push the root page; navigation continues from on-screen buttons. */
    page_manager_push(&s_manager, "HomePage", NULL);

    s_started = true;
}

void page_manager_c_demo_stop(void)
{
    if (!s_started) {
        return;
    }

    /* Clearing the stack runs each page's disappear/unload life cycle. */
    page_manager_set_stack_clear(&s_manager, false);

    /* Uninstall removes the pages from the pool and frees them via the
     * factory-created allocation (see note about on_destroy below). */
    page_manager_uninstall(&s_manager, "DetailPage");
    page_manager_uninstall(&s_manager, "HomePage");

    page_manager_deinit(&s_manager);

    s_started = false;
}
