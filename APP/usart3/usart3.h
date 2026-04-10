#ifndef USART3_H
#define USART3_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

extern volatile uint8_t control_flag;//控制标志语音模块
extern SemaphoreHandle_t control_semaphore;//通知信号量
void usart3_init(void);
void usart3_printf(const char *format, ...);

#endif /* USART3_H */
