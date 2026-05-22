#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "console.h"
#include "FreeRTOS.h"
#include "semphr.h"

/*  1、
    作为调试串口使用的
    USART2 连接方式
    PD5  -> TX
    PD6  -> RX
    3.3V -> VCC
    GND  -> GND
    2、
    按键控制初始化
    k0 -> PE4
    k1 -> PE3
    k2 -> PE2
    K3 -> PA0
*/

static console_received_func_t received_func;
static SemaphoreHandle_t write_async_semaphore;
//定义一个信号量SemaphoreHandle_t是信号量句柄，存有信号信息，初始无信息，需调用函数传递信息

static void usart2_io_init(void)
{
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
}

void button_io_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;//按键需要上拉
    GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;//PA0按键需要下拉
    GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

bool button_check(uint8_t id)
{
    bool state = 0;
    switch(id)
    {
        case 0:
            state = GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == Bit_RESET ? 1 : 0;
            break;
        case 1:
            state = GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3) == Bit_RESET ? 1 : 0;
            break;
        case 2:
            state = GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2) == Bit_RESET ? 1 : 0;
            break;
        case 3:
            state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_SET ? 1 : 0;
            break;
        default:
            return false;
    }

    if(state == 1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));//消抖
        switch (id)
        {
            case 0:
                state = GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_4) == Bit_RESET ? 1 : 0;
                break;
            case 1:
                state = GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_3) == Bit_RESET ? 1 : 0;
                break;
            case 2:
                state = GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_2) == Bit_RESET ? 1 : 0;
                break;
            case 3:
                state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == Bit_SET ? 1 : 0;
                break;
        }
    }
    return state;
}

static void usart2_init(void)
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);  
    USART_Cmd(USART2, ENABLE);
}

static void usart2_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART2->DR);//指定 DMA 要访问的外设目标地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;//指定 DMA 数据传输的方向 内存→外设
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//外设地址的自增开关
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;//指定 DMA 从内存读取数据的宽度
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;//指定 DMA 往外设写入数据的宽度
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;//传输模式 单次传输
    DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;//将 DMA 内部 FIFO 的 “触发传输阈值” 设为「FIFO 完全填满」
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;//DMA 从内存读取数据时，一次连续读取 8个数据（beats）
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;//DMA 向外部设备（如 SPI）发送数据时，每次仅传输 1 个数据
    DMA_ITConfig(DMA1_Stream6, DMA_IT_TC, ENABLE);//使能 发送完成中断
    DMA_Init(DMA1_Stream6, &DMA_InitStruct);
}

static void interrupt_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream6_IRQn;
    NVIC_Init(&NVIC_InitStructure);
}

void console_init(void)
{   
    write_async_semaphore = xSemaphoreCreateBinary();//设置二值信号  只有 0和 1两种计数状态
    //如果只声明变量而不创建信号量，write_async_semaphore 的值是 NULL。函数xSemaphoreGiveFromISR会指向非法地址
    configASSERT(write_async_semaphore);//断言 判断是否为真 否则不继续执行下去
    usart2_io_init();
    usart2_init();
    usart2_dma_init();
    interrupt_init();
}

void console_write(const char str[])
{
    if (str == NULL) return;    
    int len = strlen(str);
    do{
        uint32_t size = len < 65535 ? len : 65535;
        DMA1_Stream6->CR |= DMA_SxCR_MINC; //开启 DMA 内存地址自增
        DMA1_Stream6->M0AR = (uint32_t)str;//指定 DMA 要读取数据的内存起始地址
        DMA1_Stream6->NDTR = size;//指定 DMA 的总传输次数
        DMA_Cmd(DMA1_Stream6, ENABLE);
        xSemaphoreTake(write_async_semaphore, portMAX_DELAY);//无限期等待是否有信号量（在中断中赋值）
        str += size;
        len -= size;
    }while (len > 0);

    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
    USART_ClearFlag(USART2, USART_FLAG_TC);
}

void console_received_register(console_received_func_t func)
{
    received_func = func;
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        if (received_func != NULL)
        {
            uint8_t data = USART_ReceiveData(USART2);
            received_func(data);
        }
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    }
}

void DMA1_Stream6_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA1_Stream6, DMA_IT_TCIF6) == SET)
    {
        BaseType_t pxHigherPriorityTaskWoken;//一个布尔值（为了兼容性 不然为bool） 是一个状态标志

        //函数作用：释放信号量到write_async_semaphore。并判断是否有“更高”优先级在等待这个信号量
        //如果有就把pxHigherPriorityTaskWoken置为true
        xSemaphoreGiveFromISR(write_async_semaphore,&pxHigherPriorityTaskWoken);//在中断服务中 “释放信号量” 的专用函数/普通任务中用 xSemaphoreGive()
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);//参数为true才执行任务切换
        DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);
    }
}

int fputc (int ch,FILE*f)
{
    USART_SendData(USART2,(uint8_t) ch);
    while (USART_GetFlagStatus(USART2,USART_FLAG_TXE)==RESET);
    return ch;
}

