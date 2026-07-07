/*
 * MIT License
 * C-language port of PageFactory.
 *
 * The C++ original was a class with a virtual CreatePage() method.
 * In C, the factory is a struct holding a function pointer plus an
 * opaque user-context pointer so that the implementation can carry any
 * state it needs.
 */
#ifndef __PAGE_FACTORY_H
#define __PAGE_FACTORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "page_base.h"

typedef struct pm_factory pm_factory_t;

/* Create a page given its class name.  Return NULL if not supported. */
typedef pm_page_base_t* (*pm_factory_create_f)(pm_factory_t* self, const char* class_name);

struct pm_factory {
    pm_factory_create_f create;
    void*               user_ctx; /* optional implementation-specific context */
};

/* Helper that simply calls factory->create(factory, class_name). */
pm_page_base_t* page_factory_create_page(pm_factory_t* factory, const char* class_name);

#ifdef __cplusplus
}
#endif

#endif /* !__PAGE_FACTORY_H */