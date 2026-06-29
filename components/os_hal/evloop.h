#ifndef __EVLOOP_H__
#define __EVLOOP_H__

#include <stdint.h>

void *evloop_create(void);
void evloop_destroy(void *e);

void evloop_loop(void *e);
void evloop_quit(void *e);

typedef void (*evloop_fun_t)(void *usr_data);
void evloop_run(void *e, evloop_fun_t f, evloop_fun_t destroy, void *usr_data);

uint64_t evloop_timer_create(void *e, unsigned int interval, unsigned int repeat, evloop_fun_t f, evloop_fun_t destroy, void *usr_data);
void evloop_timer_cancel(void *e, uint64_t timer_id);

// epoll evts
//typedef void (*evloop_io_fun_t)(void *usr, int fd, int evts);
//void evloop_set_fd(void *e, int fd, int evts, evloop_io_fun_t f, evloop_fun_t destroy, void *usr);
//void evloop_del_fd(void *e, int fd);

// evloop_threads
void *evloop_thread_create2(const char *name, int stacksize, int prority);

void *evloop_thread_create(void);
void evloop_thread_delete(void *e);

void *evloop_thread_evloop(void *e);


#endif
