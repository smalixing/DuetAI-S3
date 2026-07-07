/*
 * Example usage of PageManager C version
 * This file demonstrates how to use the PageManager C implementation
 */

#include "page_manager.h"
#include "page_base.h"
#include "page_factory.h"
#include <stdio.h>
#include <string.h>

/* Example page implementation: embeds pm_page_base_t as its first member so a
 * my_page_t* can be freely cast to/from pm_page_base_t*. */
typedef struct {
    pm_page_base_t base;
    int custom_data;
} my_page_t;

/* ------------------------------------------------------------------ */
/* Page life-cycle callbacks (the page's "virtual" methods)            */
/* ------------------------------------------------------------------ */

/**
 * @brief  Called when the page's UI is being created (load start).
 * @param  self  Pointer to the page base (cast to my_page_t* for own fields).
 * @retval None
 */
static void my_page_on_view_load(pm_page_base_t* self) {
    printf("MyPage: on_view_load called\n");
}

/**
 * @brief  Called just before the page's appear animation starts.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void my_page_on_view_will_appear(pm_page_base_t* self) {
    printf("MyPage: on_view_will_appear called\n");
}

/**
 * @brief  Called after the page's appear animation has finished.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void my_page_on_view_did_appear(pm_page_base_t* self) {
    printf("MyPage: on_view_did_appear called\n");
}

/**
 * @brief  Called just before the page's disappear animation starts.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void my_page_on_view_will_disappear(pm_page_base_t* self) {
    printf("MyPage: on_view_will_disappear called\n");
}

/**
 * @brief  Called after the page's disappear animation has finished.
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void my_page_on_view_did_disappear(pm_page_base_t* self) {
    printf("MyPage: on_view_did_disappear called\n");
}

/**
 * @brief  Called when the page's UI is being torn down (unload start).
 * @param  self  Pointer to the page base.
 * @retval None
 */
static void my_page_on_view_unload(pm_page_base_t* self) {
    printf("MyPage: on_view_unload called\n");
}

/**
 * @brief  Called by Uninstall() after unload so the page can free itself.
 * @param  self  Pointer to the page base; free the owning my_page_t here.
 * @retval None
 */
static void my_page_on_destroy(pm_page_base_t* self) {
    printf("MyPage: on_destroy called\n");
}

/* Page vtable: maps the life-cycle events to the callbacks above.
 * Unset entries default to NULL and are treated as no-ops. */
static const pm_page_vtable_t my_page_vtable = {
    .on_view_load = my_page_on_view_load,
    .on_view_will_appear = my_page_on_view_will_appear,
    .on_view_did_appear = my_page_on_view_did_appear,
    .on_view_will_disappear = my_page_on_view_will_disappear,
    .on_view_did_disappear = my_page_on_view_did_disappear,
    .on_view_unload = my_page_on_view_unload,
    .on_destroy = my_page_on_destroy,
};

/**
 * @brief  Factory callback: construct a my_page_t for a given class name.
 * @note   Allocates the page, initialises its base with the shared vtable,
 *         sets its name and custom data, and returns the embedded base.
 *         Returns NULL for unknown class names.
 * @param  factory     The factory instance (unused here).
 * @param  class_name  Requested page class name.
 * @retval Pointer to the new page's base, or NULL if unsupported / OOM.
 */
static pm_page_base_t* my_page_factory_create(pm_factory_t* factory, const char* class_name) {
    if (strcmp(class_name, "MyPage") == 0) {
        my_page_t* page = (my_page_t*)lv_mem_alloc(sizeof(my_page_t));
        if (page) {
            page_base_init(&page->base, &my_page_vtable);
            page->base.name = "MyPage";
            page->custom_data = 123;
            return &page->base;
        }
    }
    return NULL;
}

/**
 * @brief  End-to-end demonstration of the PageManager C API.
 * @note   Builds a factory, initialises a manager, installs two page
 *         instances of the same class, then exercises Push/Pop/Replace/
 *         BackHome navigation before tearing everything down with DeInit.
 * @retval None
 */
void test_page_manager(void) {
    printf("=== PageManager C Version Test ===\n");
    
    // Create factory
    pm_factory_t factory = {
        .create = my_page_factory_create,
        .user_ctx = NULL,
    };
    
    // Initialize page manager
    pm_manager_t manager;
    page_manager_init(&manager, &factory);
    
    // Set global animation
    page_manager_set_global_load_anim_type(&manager, PM_LOAD_ANIM_OVER_LEFT, 500, lv_anim_path_ease_out);
    
    // Install pages
    page_manager_install(&manager, "MyPage", "MainPage");
    page_manager_install(&manager, "MyPage", "SettingsPage");
    
    // Navigate
    printf("\n--- Push MainPage ---\n");
    page_manager_push(&manager, "MainPage", NULL);
    
    printf("\n--- Push SettingsPage ---\n");
    page_manager_push(&manager, "SettingsPage", NULL);
    
    printf("\n--- Pop back ---\n");
    page_manager_pop(&manager);
    
    printf("\n--- Replace with SettingsPage ---\n");
    page_manager_replace(&manager, "SettingsPage", NULL);
    
    printf("\n--- Back to home ---\n");
    page_manager_back_home(&manager);
    
    printf("\n=== Test Complete ===\n");
    
    // Cleanup
    page_manager_deinit(&manager);
}