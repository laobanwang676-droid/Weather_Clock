#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "work_queue.h"

typedef struct {
    WorkFunction_t function;
    void *param;
} Workqueue_t;

static QueueHandle_t workQueue;

void work_func(void *param)
{
    Workqueue_t message;
    while(1)
    {
        xQueueReceive(workQueue, &message, portMAX_DELAY);
        if(message.function != NULL)
        {
            message.function(message.param);
        }
    }
}

void work_send(WorkFunction_t function, void *param)
{
    Workqueue_t message;
    message.function = function;
    message.param = param;
    xQueueSend(workQueue, &message, portMAX_DELAY);
}

void work_queue_init(void)
{
    workQueue = xQueueCreate(10, sizeof(Workqueue_t));
    configASSERT(workQueue != NULL);
    xTaskCreate(work_func, "work_func", 512, NULL, 7, NULL);
}
