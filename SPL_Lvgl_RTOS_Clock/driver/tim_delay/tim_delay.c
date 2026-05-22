#include <stdint.h>
#include <string.h>
#include "tim_delay.h"
#include "stm32f4xx.h"
#include "lvgl.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"

#define TIM6_PRESCALER   (83)        // 84MHz / (83+1) = 1MHz
#define TICKS_PER_US     (1)         // 1 tick = 1us
#define TICKS_PER_MS     (1000)      // 1ms = 1000 ticks
#define TIM6_AUTO_RELOAD (999)       // 1ms中断一次

static volatile uint64_t tim_tick_count;
static tim_periodic_callback_t periodic_callback;

void tim_tick_init(void)
{ 
    // 初始化TIM6结构体
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Prescaler = TIM6_PRESCALER;// 预分频值
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;// 向上计数模式
    TIM_TimeBaseStructure.TIM_Period = TIM6_AUTO_RELOAD;// 自动重装载值
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);
    
    // 清除中断标志，初始化时可能产生一个更新中断
    TIM_ClearFlag(TIM6, TIM_FLAG_Update);
    // 使能中断
    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);
    
    // 配置NVIC
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    // 启动定时器
    TIM_Cmd(TIM6, ENABLE);
}

uint64_t tim_now(void)
{
    uint64_t now, last_count;
    uint16_t cnt;
    
    do {
        last_count = tim_tick_count;
        cnt = TIM_GetCounter(TIM6);
        
        // 检查是否发生更新但未处理
        if (TIM_GetFlagStatus(TIM6, TIM_FLAG_Update) != RESET)
        {
            cnt = TIM_GetCounter(TIM6);
            now = last_count + TICKS_PER_MS + cnt;
        } else 
        {
            now = last_count + cnt;
        }
    } while (last_count != tim_tick_count);
    
    return now;
}

uint64_t tim_get_us(void)
{
    return tim_now() / TICKS_PER_US;
}

uint64_t tim_get_ms(void)
{
    return tim_now() / TICKS_PER_MS;
}

void tim_delay_us(uint32_t us)
{
    uint64_t now = tim_now();
    while (tim_now() - now < (uint64_t)us * TICKS_PER_US);
}

void tim_delay_ms(uint32_t ms)
{
    uint64_t now = tim_now();
    while (tim_now() - now < (uint64_t)ms * TICKS_PER_MS);
}

void function_address_passing(tim_periodic_callback_t callback)
{
    periodic_callback = callback;
}

// TIM6中断服务函数
void TIM6_DAC_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)
     {
        TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
        tim_tick_count += TICKS_PER_MS;
        lv_tick_inc(1); // 通知LVGL增加1ms的系统时间
        if (periodic_callback) 
        {
            periodic_callback();
        }
    }
}
