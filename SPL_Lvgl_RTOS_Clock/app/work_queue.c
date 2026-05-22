#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "work_queue.h"
#include "queue.h"

typedef struct work_queue
{
    work_func_t work;
    void *param;
}work_message_t;

static QueueHandle_t work_queue;

static void work_func(void* param)
{   
    work_message_t work_msg;
    while(1)
    {
        xQueueReceive(work_queue, &work_msg, portMAX_DELAY);//从队列中接收消息，阻塞等待
        work_msg.work(work_msg.param);//执行接收到的工作函数
    }
}

void work_queue_init(void)
{
    work_queue = xQueueCreate(16, sizeof(work_message_t));//创建一个队列，最多容纳16个work_message_t类型的元素
    configASSERT(work_queue);//断言队列创建成功
    xTaskCreate(work_func, "work_queue_task", 1024, NULL, 6, NULL);
}

void work_queue_send(work_func_t work, void* param)
{
    work_message_t work_msg;
    work_msg.work = work;
    work_msg.param = param;
    xQueueSend(work_queue, &work_msg, portMAX_DELAY);//将消息发送到队列，阻塞等待
}
