#include <stdint.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "app.h"
#include "esp32.h"
#include "rtc.h"
#include "aht20.h"
#include "tim_delay.h"
#include "console.h"
#include "usart3.h"
#include "FreeRTOS.h"
#include "task.h"

void board_rcc_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);  
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);//lcd显示控制
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE); //esp32控制命令发送
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);//调试信息输出
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);//aht20初始化
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);//初始化rtc
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);//初始化
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);//初始化
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//设置中断优先级分组4
    PWR_BackupAccessCmd(ENABLE);
    RCC_LSEConfig(ENABLE);
    while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
}

static void beep_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode=GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType=GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd=GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed=GPIO_Low_Speed;
    GPIO_InitStruct.GPIO_Pin=GPIO_Pin_0;
    GPIO_Init(GPIOB,&GPIO_InitStruct);
}

void beep_work(void)
{
    for(uint8_t i=0;i<2;i++)
    {
        GPIO_SetBits(GPIOB,GPIO_Pin_0);
        vTaskDelay(pdMS_TO_TICKS (500)); 
        GPIO_ResetBits(GPIOB,GPIO_Pin_0);
        vTaskDelay(pdMS_TO_TICKS (500));
    }
}

void board_init(void)
{
    esp32_init();
    console_init();
    beep_init();
    tim_tick_init();
    // st7789_init();
    rtc_init();
    aht20_Init();
    usart3_init();
}

void vAssertCalled(const char *file, int line)
{
    printf("Assert Called: %s(%d)\n", file, line);
    configASSERT(0);
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName)
{
    printf("Stack Overflowed: %s\n", pcTaskName);
    configASSERT(0);
}

void vApplicationMallocFailedHook( void )
{
    printf("Malloc Failed\n");
    configASSERT(0);
}
