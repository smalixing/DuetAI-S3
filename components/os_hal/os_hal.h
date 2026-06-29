#ifndef __OS_HAL_H__
#define __OS_HAL_H__

#include <stdint.h>

uint64_t hal_timestamp_get(void);

void *hal_mutex_create(const char *name);
void hal_mutex_destroy(void *mux);
int hal_mutex_lock(void *mux);
int hal_mutex_unlock(void *mux);

void *hal_queue_create(const char *name, int item_size, int queue_len);
void hal_queue_destroy(void *queue);
int hal_queue_send(void *queue, void *item, int timeout);
int hal_queue_recv(void *queue, void *item, int timeout);

void *hal_thread_create(const char *name, void (*thread_func)(void *), void *param, int stack_size, int prority);
void *hal_thread_self();
void hal_thread_delete(void *thread);
void hal_thread_sleep(uint32_t ms);

void *hal_event_create(const char *name);
void hal_event_destroy(void *event);
int hal_event_set(void *event, uint32_t evt_bits);
uint32_t hal_event_wait(void *event, uint32_t evt_bits, int clear, int wait_all, int timeout);
uint32_t hal_event_clear(void *event, uint32_t clear_bits);

uint64_t hal_timestamp_get(void);

#endif
