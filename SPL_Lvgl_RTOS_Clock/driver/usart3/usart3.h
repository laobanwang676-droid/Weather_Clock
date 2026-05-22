#ifndef USART3_H__
#define USART3_H__

#include "FreeRTOS.h"
#include "semphr.h"

void usart3_init(void);
void usart3_printf(const char* str, ...);
extern volatile uint8_t control_flag;//用于控制是否发送数据
extern SemaphoreHandle_t control_semaphore;//用于控制任务的信号量
extern SemaphoreHandle_t dma_complete_semaphore;//用于dma传输完成的信号量
#endif /* USART3_H__ */
