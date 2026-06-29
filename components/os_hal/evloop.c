#include "evloop.h"
#include "os_hal.h"
#include "hal_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct _timer_s {
    struct _timer_s *next;
    void *evloop;
    uint32_t tp;
    unsigned int interval;
    unsigned int repeat;
    evloop_fun_t f;
    evloop_fun_t destroy;
    void *usr_data;
    uint64_t id;
} etimer_t;

typedef struct _fun_s {
    evloop_fun_t f;
    evloop_fun_t destroy;
    void *usr_data;
    struct _fun_s *next;
} fun_t;

typedef struct {
    void *mux;
    void *evt;
    etimer_t *timer_list;
    fun_t *fun_list;
    fun_t *end_list;
    void *pid;
    int timer_running;
    uint64_t timer_id;
} evloop_t;

#define EVLOOP_EVT_CMD_WAKEUP   0x01
#define EVLOOP_EVT_CMD_STOP     0x02
#define EVLOOP_EVT_RUNNING      0x04
#define EVLOOP_EVT_QUIT         0x08


void *evloop_create(void)
{
    evloop_t *e = malloc(sizeof(evloop_t));
    assert(e != NULL);
    e->mux = hal_mutex_create("evloop");
    assert(e->mux != NULL);
    e->evt = hal_event_create("evloop");
    assert(e->evt != NULL);
    e->timer_list = NULL;
    e->fun_list = NULL;
    e->end_list = NULL;
    e->pid = hal_thread_self();
    e->timer_running = 0;
    e->timer_id = 0;
    return e;
}

void evloop_destroy(void *p)
{
    evloop_t *e = (evloop_t *)p;
    hal_mutex_destroy(e->mux);
    hal_event_destroy(e->evt);

    while (e->timer_list) {
        etimer_t *t = e->timer_list;
        e->timer_list = t->next;
        if (t->destroy)
            t->destroy(t->usr_data);
        free(t);
    }

    while (e->fun_list) {
        fun_t *f = e->fun_list;
        e->fun_list = f->next;
        if (f->destroy)
            f->destroy(f->usr_data);
        free(f);
    }

    free(e);
}

static inline int timeout(uint32_t t1, uint32_t t2)
{
    if ((int32_t)(t1 - t2) >= 0)
        return 1;
    return 0;
}

static void add_timer(evloop_t *e, etimer_t *t)
{
    etimer_t *prev = NULL;
    etimer_t *cur = e->timer_list;

    while (cur) {
        if (timeout(cur->tp, t->tp))
            break;
        prev = cur;
        cur = cur->next;
    }

    t->next = cur;
    if (prev) {
        prev->next = t;
    } else {
        e->timer_list = t;
    }
}

static int timer_check(evloop_t *e)
{
    uint32_t tp = hal_timestamp_get();

    e->timer_running = 1;
    while (e->timer_list) {

        etimer_t *header = e->timer_list;
        if (timeout(tp, header->tp)) {
            e->timer_list = header->next;
            header->f(header->usr_data);
            if (header->repeat) {
                header->tp = hal_timestamp_get() + header->interval;
                add_timer(e, header);
            } else {
                if (header->destroy)
                    header->destroy(header->usr_data);
                free(header);
            }
        } else {
            break;
        }
    }
    e->timer_running = 0;

    if (e->timer_list) {
        tp = hal_timestamp_get();
        if (timeout(tp, e->timer_list->tp))
            return 0;
        return (e->timer_list->tp - tp);
    }

    return -1;
}

void evloop_loop(void *p)
{
    evloop_t *e = (evloop_t *)p;
    e->pid = hal_thread_self();

    while (1) {
        int timeout = timer_check(e);

        uint32_t evts = hal_event_wait(e->evt, EVLOOP_EVT_CMD_WAKEUP | EVLOOP_EVT_CMD_STOP, 1, 0, timeout);
        if (evts & EVLOOP_EVT_CMD_STOP)
            break;

        hal_mutex_lock(e->mux);
        fun_t *funs = e->fun_list;
        e->fun_list = NULL;
        e->end_list = NULL;
        hal_mutex_unlock(e->mux);

        while (funs) {
            fun_t *node = funs;
            funs = funs->next;

            node->f(node->usr_data);
            if (node->destroy)
                node->destroy(node->usr_data);
            free(node);
        }
    }
}

void evloop_quit(void *p)
{
    evloop_t *e = (evloop_t *)p;
    hal_event_set(e->evt, EVLOOP_EVT_CMD_STOP);
}

static void _queue_inloop(evloop_t *e, evloop_fun_t f, evloop_fun_t destroy, void *usr_data)
{
    fun_t *ctx = malloc(sizeof(fun_t));
    ctx->f = f;
    ctx->destroy = destroy;
    ctx->usr_data = usr_data;
    ctx->next = NULL;

    hal_mutex_lock(e->mux);
    if (e->end_list) {
        e->end_list->next = ctx;
    } else {
        hal_event_set(e->evt, EVLOOP_EVT_CMD_WAKEUP);
        e->fun_list = ctx;
    }
    e->end_list = ctx;

    hal_mutex_unlock(e->mux);
}

void evloop_run(void *p, evloop_fun_t f, evloop_fun_t destroy, void *usr_data)
{
    evloop_t *e = (evloop_t *)p;
    if (hal_thread_self() == e->pid) {
        f(usr_data);
        if (destroy)
            destroy(usr_data);
        return;
    }

    _queue_inloop(e, f, destroy, usr_data);
}

static void add_timer_inloop(void *p)
{
    etimer_t *timer = (etimer_t *)p;
    add_timer(timer->evloop, timer);
}

uint64_t evloop_timer_create(void *p, unsigned int interval, unsigned int repeat, evloop_fun_t f, evloop_fun_t destroy, void *usr_data)
{
    evloop_t *e = (evloop_t *)p;

    etimer_t *timer = malloc(sizeof(etimer_t));
    timer->interval = interval;
    timer->repeat = repeat;
    timer->f = f;
    timer->destroy = destroy;
    timer->usr_data = usr_data;
    timer->evloop = e;
    timer->tp = hal_timestamp_get() + interval;

    hal_mutex_lock(e->mux);
    uint64_t id = ++ e->timer_id;
    timer->id = id;
    hal_mutex_unlock(e->mux);

    evloop_run(e, add_timer_inloop, NULL, timer);
    return id;
}

typedef struct {
    uint64_t timer_id;
    void *e;
} del_etimer_t;

static void del_timer_inloop(void *p)
{
    del_etimer_t *d = (del_etimer_t *)p;
    evloop_t *e = (evloop_t *)d->e;

    if (e->timer_running) {
        _queue_inloop(e, del_timer_inloop, NULL, p);
        return;
    }

    uint64_t timer_id = d->timer_id;
    etimer_t *prev = NULL;
    etimer_t *cur = e->timer_list;

    while (cur) {
        if (cur->id == timer_id) {
            if (prev) {
                prev->next = cur->next;
            } else {
                e->timer_list = cur->next;
            }
            if (cur->destroy)
                cur->destroy(cur->usr_data);
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    free(p);
}

void evloop_timer_cancel(void *e, uint64_t timer_id)
{
    del_etimer_t *d = malloc(sizeof(del_etimer_t));
    d->timer_id = timer_id;
    d->e = e;
    evloop_run((evloop_t *)e, del_timer_inloop, NULL, d);
}

static void evloop_thread(void *p)
{
    evloop_loop(p);
    evloop_destroy(p);
    hal_thread_delete(NULL);
}

void *evloop_thread_create2(const char *name, int stacksize, int prority)
{
    evloop_t *e = (evloop_t *)evloop_create();
    e->pid = hal_thread_create(name, evloop_thread, e, stacksize, prority);
    return e;
}

void *evloop_thread_create()
{
    return evloop_thread_create2("evloop", 1024 * 8, 5);
}

void evloop_thread_delete(void *e)
{
    evloop_quit(e);
}

void *evloop_thread_evloop(void *e)
{
    return e;
}
