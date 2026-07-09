#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_heap_caps.h"


void *hal_mutex_create(const char *name)
{
    return xSemaphoreCreateMutex();
}

void hal_mutex_destroy(void *mux)
{
    vSemaphoreDelete(mux);
}

int hal_mutex_lock(void *mux)
{
    return !xSemaphoreTake(mux, -1);
}

int hal_mutex_unlock(void *mux)
{
    return !xSemaphoreGive(mux);
}

void *hal_queue_create(const char *name, int item_size, int queue_len)
{
    return xQueueCreate(queue_len, item_size);
}

void hal_queue_destroy(void *queue)
{
    vQueueDelete(queue);
}

int hal_queue_send(void *queue, void *item, int timeout)
{
    return !xQueueSend(queue, item, pdMS_TO_TICKS(timeout));
}

int hal_queue_recv(void *queue, void *item, int timeout)
{
    return !xQueueReceive(queue, item, pdMS_TO_TICKS(timeout));
}

void *hal_thread_create(const char *name, void (*thread_func)(void *), void *param, int stack_size, int prority)
{
    TaskHandle_t handle = NULL;
    xTaskCreatePinnedToCoreWithCaps(thread_func, name, stack_size, param, prority, &handle,
                                    tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return handle;
}

void *hal_thread_self()
{
    return xTaskGetCurrentTaskHandle();
}

void hal_thread_delete(void *thread)
{
    vTaskDeleteWithCaps(thread);
}

void hal_thread_sleep(uint32_t ms)
{
    vTaskDelay(ms / portTICK_RATE_MS);
}

void *hal_event_create(const char *name)
{
    return xEventGroupCreate();
}

void hal_event_destroy(void *event)
{
    vEventGroupDelete(event);
}

int hal_event_set(void *event, uint32_t evt)
{
    xEventGroupSetBits(event, evt);
    return 0;
}

uint32_t hal_event_wait(void *event, uint32_t evt_bits, int clear, int wait_all, int timeout)
{
    return xEventGroupWaitBits(event, evt_bits, clear, wait_all, timeout / portTICK_RATE_MS);
}

uint32_t hal_event_clear(void *event, uint32_t clear_bits)
{
    return xEventGroupClearBits(event, clear_bits);
}
