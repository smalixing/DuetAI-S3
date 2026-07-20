/*
 * Custom LVGL allocator: route lv_malloc/realloc/free to PSRAM.
 *
 * Enabled via CONFIG_LV_USE_CUSTOM_MALLOC=y. This removes LVGL's 64 KB static
 * TLSF pool from internal .bss and serves all LVGL allocations from PSRAM,
 * freeing scarce internal RAM (see docs/memory-psram.md).
 */

#include "lvgl.h"
#include "esp_heap_caps.h"

void lv_mem_init(void)
{
    /* Nothing to init: heap_caps manages the PSRAM heap for us. */
}

void lv_mem_deinit(void)
{
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void *lv_malloc_core(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    return heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void lv_free_core(void *p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    LV_UNUSED(mon_p);
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}
