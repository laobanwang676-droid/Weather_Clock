#include <stdarg.h>
#include <stdio.h>
#include "main.h"
#include "usart3.h"
#include "FreeRTOS.h"
#include "semphr.h"

/*
    USART3 连接方式
    PD8 ---- USART3_TX
    PD9 ---- USART3_RX
*/
extern DMA_HandleTypeDef hdma_usart3_tx;
extern UART_HandleTypeDef huart3;
static SemaphoreHandle_t dma_complete_semaphore;

SemaphoreHandle_t control_semaphore;//通知信号量
volatile uint8_t control_flag;//控制标志语音模块

void usart3_init(void)
{
    dma_complete_semaphore = xSemaphoreCreateBinary();
    configASSERT(dma_complete_semaphore != NULL);
    control_semaphore = xSemaphoreCreateBinary();
    configASSERT(control_semaphore != NULL);
    //使能串口中断
    HAL_UART_Receive_IT(&huart3, (uint8_t *)&control_flag, 1);
}

void usart3_printf(const char *format, ...)
{
    static char buf[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    if (len > 0)
    {
        HAL_UART_Transmit_DMA(&huart3, (uint8_t *)buf, len);
        //等待DMA传输完成
        xSemaphoreTake(dma_complete_semaphore, portMAX_DELAY);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{//DMA没有专属的回调函数，使用UART的回调函数来通知DMA传输完成。其他外设也是一样的

    if (huart->Instance == USART3)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(dma_complete_semaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
