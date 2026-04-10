#include <stdio.h>
#include <string.h>
#include "st7789.h"
#include "main.h"   
#include "font.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_work.h"
#include "ath20.h"
#include "ui.h"
#include "work_queue.h"
#include "rtc.h"
#include "usart3.h"
#include "page.h"
#include "esp32c3.h"
#include "timers.h"
#include "semphr.h"
#include "weather.h"
//            app_init = button > timer = beep > work > ui > usart3_control
//优先级                     9       8             7     6     5

void app_init(void *param)
{
    st7789_init();
    aht20_Init();
    usart3_init();
    esp32c3_init();
    welcome_page();
    vTaskDelay(pdMS_TO_TICKS(2000));
//此时timer和beep任务没启动,timer启动后work和ui才会收到信息通知。所以纯粹延时效果，cpu没有任务可运行    
    wifi_init();
    wifi_connect();
    main_page();
    app_work_init();//启动了“timer任务”和“蜂鸣器任务” 由于想让界面先显示出来所以放在后面启动
    vTaskDelete(NULL);//删除本任务
}

#define ms(x) (x)
#define seconds(x)  ms((x) * 1000)
#define minutes(x)  seconds((x) * 60)
#define hous(x)     minutes((x) * 60)  
#define days(x)     hous((x) * 24)  

#define SNTP_TIME_INTERVAL        hous(1)
#define WIFI_TIME_INTERVAL        seconds(10)
#define RTC_TIME_INTERVAL         seconds(1)
#define INNER_TIME_INTERVAL       seconds(5)    
#define OUTDOOR_TIME_INTERVAL     minutes(1)

static TaskHandle_t  hbeep_func;//蜂鸣器任务句柄
static TimerHandle_t time_sync_timer;
static TimerHandle_t wifi_sync_timer;
static TimerHandle_t rtc_sync_timer;
static TimerHandle_t inner_sync_timer;
static TimerHandle_t outdoor_sync_timer;

static weather_info_t last_weather = { 0 };
static float last_temperature, last_humidity;

static void time_sntp_sync(void)//时间同步
{   
    uint32_t time_sntp = SNTP_TIME_INTERVAL;
    esp_date_time_t esp_date = { 0 };
    if(!esp_at_sntp_get_time(&esp_date))
    {
        printf("[SNTP] get time failed\n");
        time_sntp = seconds(1);
        xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(time_sntp), 0);//重置定时周期 并立即重置
        return;
    }
    
    if (esp_date.year < 2000)
    {
        printf("[SNTP] invalid date format\n");
        xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(time_sntp_sync), 0);//重置定时周期 并立即重置
        return;
    }   
    printf("[SNTP] sync time: %04u-%02u-%02u %02u:%02u:%02u (%d)\n",
        esp_date.year, esp_date.month, esp_date.day,
        esp_date.hour, esp_date.minute, esp_date.second, esp_date.weekday);
    
    rtc_date_time_t rtc_date = { 0 };
    rtc_date.year = esp_date.year;
    rtc_date.month = esp_date.month;
    rtc_date.day = esp_date.day;
    rtc_date.hour = esp_date.hour;
    rtc_date.minute = esp_date.minute;
    rtc_date.second = esp_date.second;
    rtc_date.weekday = esp_date.weekday;
    
    rtc_set_time(&rtc_date);  
    xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(SNTP_TIME_INTERVAL), 0);//重置定时周期 并立即重置
}

static void wifi_sync(void)//wifi同步
{
    static wifi_info_t last_info = { 0 };//用于保存当前wifi状态
    
    xTimerChangePeriod(wifi_sync_timer, pdMS_TO_TICKS(WIFI_TIME_INTERVAL), 0);//重置定时周期 并立即重置
    
    wifi_info_t wifi_info = { 0 };
    if(!esp_at_get_wifi_info(&wifi_info))
    {
        main_page_redraw_ssid("wifi get error!!!");
        return;
    }
        
   if(memcmp(&wifi_info, &last_info, sizeof(wifi_info_t)) != 0)//上电第一次判断肯定是不相等的
    {
        printf("wifi connected to :%s\n", wifi_info.ssid);
        printf("wifi ssid: %s, bssid: %s, channel: %d, rssid: %d\n", 
                wifi_info.ssid, wifi_info.bssid, wifi_info.channel, wifi_info.rssi);
        main_page_redraw_ssid(wifi_info.ssid);
    }
    else
    {
        return;
    }
    memcpy(&last_info, &wifi_info, sizeof(wifi_info_t));
}

static void time_rtc_sync(void)
{
    static rtc_date_time_t last_time = { 0 };

    xTimerChangePeriod(rtc_sync_timer, pdMS_TO_TICKS(RTC_TIME_INTERVAL), 0);//重置定时周期 并立即重置
    rtc_date_time_t rtc_date;
    rtc_get_time(&rtc_date);//在time_sntp_sync函数已经把获取的时间赋给rtc了这里可以直接获取

    if (rtc_date.year < 2000)
    {
        return;
    }
    
    if(memcmp(&last_time, &rtc_date, sizeof(rtc_date_time_t)) == 0)
    {
        return;
    }
    
    memcpy(&last_time, &rtc_date, sizeof(rtc_date_time_t));
    main_page_redraw_date(&rtc_date);
    main_page_redraw_time(&rtc_date);
}

static void inner_sync(void)
{
    xTimerChangePeriod(inner_sync_timer, pdMS_TO_TICKS(INNER_TIME_INTERVAL), 0);//重置定时周期 并立即重置

    if(!aht20_start_measure())
    {
        printf("aht20 start falied!\n");
        main_page_redraw_inner_temperature(-99.0f);
        main_page_redraw_inner_humidity(-99.0f);
        return;
    }
    
    if(!aht20_wait_measure())
    {
        printf("aht20_wait failed\n");
        main_page_redraw_inner_temperature(-99.0f);
        main_page_redraw_inner_humidity(-99.0f);
        return;
    }
    float temperature = 0.0f, humidity = 0.0f;
    if(!aht20_read_measure(&temperature, &humidity))
    {
        printf("[AHT20] read measurement failed\n");
        main_page_redraw_inner_temperature(-99.0f);
        main_page_redraw_inner_humidity(-99.0f);
        return;
    }
    
    if(temperature == last_temperature && humidity == last_humidity)
    {
        return;
    }

    last_temperature = temperature;
    last_humidity = humidity;
    
    printf("[AHT20] Temperature: %.1f, Humidity: %.1f\n", temperature, humidity);

    if(humidity > 85.0f || humidity < 30.0f)
    {
        xTaskNotify(hbeep_func, 1, eSetValueWithOverwrite);//通知蜂鸣器任务执行蜂鸣,且覆盖之前的通知值
        //参数分别为 任务句柄 通知值 操作方式
    }

    main_page_redraw_inner_temperature(temperature);
    main_page_redraw_inner_humidity(humidity);
}

static void outdoor_sync(void)
{
    xTimerChangePeriod(outdoor_sync_timer, pdMS_TO_TICKS(OUTDOOR_TIME_INTERVAL), 0);//重置定时周期 并立即重置
    
    weather_info_t weather = { 0 };
    const char *weather_url = "https://api.seniverse.com/v3/weather/now.json?key=SUJs_gBrmck4GWVzV&location=Nanchong&language=en&unit=c";
    const char *weather_http_info = esp_at_http_get(weather_url);//网络访问请求
    
    if(weather_http_info == NULL)
    {
        printf("[WEATHER] http error\n");
        return;
    }
    
    if(!parse_seniverse_response(weather_http_info, &weather))
    {
        printf("[WEATHER] parse failed\n");
        return;
    }   
    memcpy(&last_weather, &weather, sizeof(weather_info_t));
    printf("[WEATHER] %s, %s, %.1f\n, %d\n", weather.city, weather.weather, 
                                weather.temperature, weather.weather_code);
    
    main_page_redraw_outdoor_weather_icon(weather.weather_code);
    main_page_redraw_outdoor_temperature(weather.temperature);
}

void beep_work(void)
{
    for(uint8_t i=0;i<2;i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS (500)); 
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS (500));
    }
}

static void beep_func(void* param)//蜂鸣器任务
{
    while(1)
    {   
        uint32_t event = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);//等待通知值，pdTRUE获取后清零通知值
        if(event)
        {
            beep_work();
        }
    }
}

static void smartconfig_task(void)//配网函数
{
    main_page_redraw_ssid("smartconfig...");
    if(!esp_at_smartconfig()) //立即执行一次配网模式
        main_page_redraw_ssid("smartconfig failed");
    return;          
}

static void smartconfig_stop_task(void)//停止配网函数
{
    if(!esp_at_stop_smartconfig()) //立即执行一次停止配网
        main_page_redraw_ssid("stop failed");
    return;          
}

static void usart3_control(void* param)
{
    while(1)
    {
        xSemaphoreTake(control_semaphore, portMAX_DELAY);//等待信号量
        printf("[TTS] semaphore taken, control_flag=0x%02x\n", control_flag);
        switch(control_flag)
        {
            case 0x01:
                vTaskDelay(pdMS_TO_TICKS (3000));
                usart3_printf("<G>当前室外温度为%.1f摄氏度,室内温度为%.1f摄氏度,室内湿度为百分之%.1f",
                    last_weather.temperature,last_temperature,last_humidity);
                break;
            case 0x02:
                vTaskDelay(pdMS_TO_TICKS(3000));
                usart3_printf("<G>当前室内环境温度为%.1f摄氏度,湿度为百分之%.1f",
                    last_temperature,last_humidity);
                break;
            case 0x03:
                vTaskDelay(pdMS_TO_TICKS(3000));
                usart3_printf("<G>当前室内环境温度为%.1f摄氏度",
                    last_temperature);
                break;
            case 0x04:
                vTaskDelay(pdMS_TO_TICKS(3000));
                usart3_printf("<G>当前室内环境湿度为百分之%.1f",
                    last_humidity);
                break;
            default:
                break;
        }
    }
}

typedef void (*sync_job)(void);
static void app_job(void* param)//timer回调函数，定时到调用
{
    sync_job job = (sync_job)param;
    job();
}

static void timer_cb(TimerHandle_t xTimer)//timer回调函数，定时到调用
{
    sync_job job = (sync_job)pvTimerGetTimerID(xTimer);
    work_send(app_job, (void*)job);
    //static void app_job仍然意味着别的翻译单元不能直接用 app_job 名称
    //（也不能直接取它的地址），只有在本文件里取地址并传出才行。
}

static void timer_rtc_cb( TimerHandle_t xTimer)
{
    sync_job job = (sync_job)pvTimerGetTimerID(xTimer);
    job();
}

static void button_task(void* param)//按键扫描任务
{
    TickType_t last_wake_time = xTaskGetTickCount();//使用更加精确的时间控制按键扫描频率 
    //避免vTaskDelay的累积误差导致按键扫描频率过低或者过高
    const TickType_t debounce_delay = pdMS_TO_TICKS(200);//固定200ms执行一次
    while(1)
    {
        if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)
            {
                printf("[BTN] K0 pressed (PE4)\n");
                work_send(app_job, (void*)time_sntp_sync);//立即执行一次时间同步
                while(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET)//避免按键长按时重复触发 只有等按键释放后才允许下一次触发
                { 
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET)
            {
                printf("[BTN] K1 pressed (PE3)\n");
                work_send(app_job, (void*)outdoor_sync);//立即执行一次室外环境同步
                while(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET) 
                { 
                    vTaskDelay(pdMS_TO_TICKS(50)); 
                }
            }
        }
        if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET)
            {
                printf("[BTN] K2 pressed (PE2)\n");
                work_send(app_job, (void*)smartconfig_task);//立即执行一次配网模式
                while(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET)
                { 
                    vTaskDelay(pdMS_TO_TICKS(50)); 
                }
            }
        }
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
            {
                printf("[BTN] K3 pressed (PA0)\n");
                work_send(app_job, (void*)smartconfig_stop_task);//立即执行一次停止配网模式
                while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
                { 
                    vTaskDelay(pdMS_TO_TICKS(50)); 
                }
            }
        }
        vTaskDelayUntil(&last_wake_time, debounce_delay);
        
    }
}

void app_work_init(void)
{   
    xTaskCreate(beep_func, "beep task", 128, NULL, 8, &hbeep_func);
    xTaskCreate(usart3_control, "usart3 control", 256, NULL, 5, NULL);

    time_sync_timer = xTimerCreate("time sync", pdMS_TO_TICKS(SNTP_TIME_INTERVAL), pdTRUE,time_sntp_sync, timer_cb);
    wifi_sync_timer = xTimerCreate("wifi sync", pdMS_TO_TICKS(WIFI_TIME_INTERVAL), pdTRUE, wifi_sync, timer_cb);
    rtc_sync_timer = xTimerCreate("rtv sync", pdMS_TO_TICKS(RTC_TIME_INTERVAL), pdTRUE, time_rtc_sync, timer_rtc_cb);
    inner_sync_timer = xTimerCreate("inner sync", pdMS_TO_TICKS(INNER_TIME_INTERVAL), pdTRUE, inner_sync,timer_cb);
    outdoor_sync_timer = xTimerCreate("outdoor sync", pdMS_TO_TICKS(OUTDOOR_TIME_INTERVAL), pdTRUE, outdoor_sync, timer_cb);
    work_send(app_job, (void*)wifi_sync);
    work_send(app_job, (void*)inner_sync);
    work_send(app_job, (void*)outdoor_sync);
    work_send(app_job, (void*)time_sntp_sync);//启动时立即执行一次时间同步

    xTimerStart(time_sync_timer, 0);//启动定时器 并立即启动
    xTimerStart(wifi_sync_timer, 0);
    xTimerStart(rtc_sync_timer, 0);
    xTimerStart(inner_sync_timer, 0);
    xTimerStart(outdoor_sync_timer, 0);

    xTaskCreate(button_task, "button task", 128, NULL, 9, NULL);
}
