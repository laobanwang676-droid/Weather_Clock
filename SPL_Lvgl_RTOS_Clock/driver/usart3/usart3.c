#include <stdarg.h>
#include <string.h>
#include <stdio.h>  
#include "stm32f4xx.h"
#include "usart3.h"
#include "FreeRTOS.h"
#include "semphr.h"

/*
    USART3 连接方式
    PD8 ---- USART3_TX
    PD9 ---- USART3_RX
*/
static SemaphoreHandle_t dma_complete_semaphore;
SemaphoreHandle_t control_semaphore;//通知信号量

volatile uint8_t control_flag;//控制标志语音模块

static void gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE); 
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9; //PD8,PD9
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;          
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;      
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;         
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;          
    GPIO_Init(GPIOD, &GPIO_InitStructure);                
}

static void usart3_func_init(void)
{
    USART_InitTypeDef USART_InitStructure;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE); 
    USART_StructInit(&USART_InitStructure);                 
    USART_InitStructure.USART_BaudRate = 9600;              
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; 
    USART_InitStructure.USART_StopBits = USART_StopBits_1;      
    USART_InitStructure.USART_Parity = USART_Parity_No;        
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; 
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; 
    USART_Init(USART3, &USART_InitStructure);              
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);          
    USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE);         
    USART_Cmd(USART3, ENABLE);                              
}

static void usart3_dma_init(void)//DMA1_Stream3 usar3 tx
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART3->DR);
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_ITConfig(DMA1_Stream3, DMA_IT_TC, ENABLE);
    DMA_Init(DMA1_Stream3, &DMA_InitStruct);
}

static void interrupt_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;           
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;    
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;          
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;             
    NVIC_Init(&NVIC_InitStructure);                              

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream3_IRQn;      
    NVIC_Init(&NVIC_InitStructure);                              
}

void usart3_init(void)
{
    dma_complete_semaphore = xSemaphoreCreateBinary();//创建一个二值信号量 用于等待DMA传输完成。二值信号量只有两个状态：可用和不可用。
    configASSERT(dma_complete_semaphore);
    control_semaphore = xSemaphoreCreateBinary();//创建一个二值信号量 用于控制任务通知
    configASSERT(control_semaphore);
    gpio_init();
    usart3_func_init();
    usart3_dma_init();
    interrupt_init();
}

void usart3_printf(const char* str, ...)
{
    va_list args;
    static char buffer[256];
    va_start(args, str);
    vsnprintf(buffer, sizeof(buffer), str, args);
    va_end(args);

    uint16_t len = strlen(buffer);
    DMA_Cmd(DMA1_Stream3, DISABLE);
    while (DMA1_Stream3->CR & DMA_SxCR_EN) { }
    DMA_ClearITPendingBit(DMA1_Stream3, DMA_IT_TCIF3);
    DMA1_Stream3->NDTR = len;
    DMA1_Stream3->M0AR = (uint32_t)buffer;
    DMA_Cmd(DMA1_Stream3, ENABLE);

    xSemaphoreTake(dma_complete_semaphore, portMAX_DELAY);
}

void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        BaseType_t CONTROL_Woken;
        control_flag = USART_ReceiveData(USART3); 
        xSemaphoreGiveFromISR(control_semaphore, &CONTROL_Woken);
        portYIELD_FROM_ISR(CONTROL_Woken);
        USART_ClearITPendingBit(USART3, USART_IT_RXNE); 
    }
}

void DMA1_Stream3_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_Stream3, DMA_IT_TCIF3) != RESET)
    {
        BaseType_t TC_Woken;
        xSemaphoreGiveFromISR(dma_complete_semaphore, &TC_Woken);
        portYIELD_FROM_ISR(TC_Woken);
        DMA_ClearITPendingBit(DMA1_Stream3, DMA_IT_TCIF3);
    }
}
