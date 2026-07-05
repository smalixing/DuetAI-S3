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

#include "PageBase.h"

typedef struct PageFactory PageFactory_t;

/* Create a page given its class name.  Return NULL if not supported. */
typedef PageBase_t* (*PageFactoryCreate_f)(PageFactory_t* self, const char* class_name);

struct PageFactory {
    PageFactoryCreate_f create;
    void*               user_ctx; /* optional implementation-specific context */
};

/* Helper that simply calls factory->create(factory, class_name). */
PageBase_t* page_factory_create_page(PageFactory_t* factory, const char* class_name);

#ifdef __cplusplus
}
#endif

#endif /* !__PAGE_FACTORY_H */