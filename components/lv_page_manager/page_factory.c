/*
 * MIT License
 * C-language port of PageFactory implementation.
 */
#include "page_factory.h"

/**
 * @brief  Create a page instance for a given class name via the factory.
 * @note   Thin dispatcher that forwards to the factory's create callback,
 *         passing the factory itself so the implementation can reach its
 *         user_ctx. Safe to call with a NULL factory or NULL callback.
 * @param  factory     Pointer to the page factory (may be NULL).
 * @param  class_name  Class name identifying which page to construct.
 * @retval Pointer to a newly created page, or NULL if unsupported / bad args.
 */
pm_page_base_t* page_factory_create_page(pm_factory_t* factory, const char* class_name)
{
    if (factory == NULL || factory->create == NULL) {
        return NULL;
    }
    return factory->create(factory, class_name);
}