/*
 * MIT License
 * PageManager_C usage demo - public entry point.
 *
 * This demo shows how to use the C port of PageManager end to end:
 *   - implementing concrete pages with a PageVTable_t,
 *   - wiring them up through a PageFactory_t,
 *   - installing pages and driving navigation (Push / Pop / Replace / BackHome),
 *   - passing parameters between pages via the stash mechanism.
 *
 * It builds real LVGL UI (buttons/labels) so the page life-cycle and the
 * switch animations can be observed on screen, and is driven entirely by
 * touch/click events - no polling loop of its own.
 */
#ifndef __PAGE_MANAGER_C_DEMO_H
#define __PAGE_MANAGER_C_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Start the PageManager_C demo.
 * @note   Creates a page manager (with the demo factory), installs the demo
 *         pages, applies a global slide animation, and pushes the Home page.
 *         All further navigation is triggered by the on-screen buttons. The
 *         manager and pages live until page_manager_c_demo_stop() is called.
 * @retval None
 */
void page_manager_c_demo_start(void);

/**
 * @brief  Stop the demo and release everything it created.
 * @note   Clears the page stack (running each page's unload life cycle),
 *         uninstalls the pages and de-inits the manager. Safe to call even if
 *         the demo was never started (no-op in that case).
 * @retval None
 */
void page_manager_c_demo_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* !__PAGE_MANAGER_C_DEMO_H */
