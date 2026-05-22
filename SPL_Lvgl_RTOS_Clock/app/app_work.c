#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp32.h"
#include "aht20.h"
#include "usart3.h"
#include "lvgl_ui.h"
#include "app.h"
#include "rtc.h"
#include "weather.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "work_queue.h"
#include "timers.h"

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

static uint8_t clicked_flags = 0;//按钮点击标志位

static void time_sntp_sync(void)//时间同步
{   
    esp_date_time_t esp_date = { 0 };
    if(!esp_at_sntp_get_time(&esp_date))
    {
        printf("[SNTP] get time failed\n");
        return;
    }
    
    if (esp_date.year < 2000)
    {
        printf("[SNTP] invalid date format\n");
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
}

static void wifi_sync(void)//wifi同步
{
    static esp_wifi_info_t last_info = { 0 };//用于保存当前wifi状态
    esp_wifi_info_t wifi_info = { 0 };
    if(!esp_at_get_wifi_info(&wifi_info))
    {
        ui_send_queue(label_wifi_info, "wifi get error!!!");
        printf("[WIFI] get wifi info failed\n");
        return;
    }
        
   if(memcmp(&wifi_info, &last_info, sizeof(esp_wifi_info_t)) != 0)//上电第一次判断肯定是不相等的
    {
        printf("wifi connected to :%s\n", wifi_info.ssid);
        printf("wifi ssid: %s, bssid: %s, channel: %d, rssid: %d\n", 
                wifi_info.ssid, wifi_info.bssid, wifi_info.channel, wifi_info.rssi);
        ui_send_queue(label_wifi_info, wifi_info.ssid);
    }
    else
    {
        return;
    }
    memcpy(&last_info, &wifi_info, sizeof(esp_wifi_info_t));
}

static void time_rtc_sync(void)
{
    static rtc_date_time_t last_time = { 0 };
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

    char buf_time[6], buf_date[16], s;
    s = rtc_date.second % 2 == 0 ? ':' : ' '; // 每秒闪烁一次冒号
    snprintf(buf_time, sizeof(buf_time), "%02d%c%02d", rtc_date.hour, s, rtc_date.minute);
    snprintf(buf_date, sizeof(buf_date), "%04d-%02d-%02d %s", rtc_date.year, rtc_date.month,
                                         rtc_date.day, rtc_date.weekday == 1 ? "Mon"
                                            : rtc_date.weekday == 2 ? "Tue"
                                            : rtc_date.weekday == 3 ? "Wed"
                                            : rtc_date.weekday == 4 ? "Thu"
                                            : rtc_date.weekday == 5 ? "Fri"
                                            : rtc_date.weekday == 6 ? "Fri"
                                            : rtc_date.weekday == 7 ? "Sun" : ""); 
    ui_send_queue(label_time, buf_time);
    ui_send_queue(label_date, buf_date);
}

static void inner_sync(void)
{
    if(!aht20_start_measure())
    {
        printf("aht20 start falied!\n");
        ui_send_queue(label_indoor_value, "--.--");
        ui_send_queue(label_humidity_value, "--.--");
        return;
    }
    
    if(!aht20_wait_measure())
    {
        printf("aht20_wait failed\n");
        ui_send_queue(label_indoor_value, "--.--");
        ui_send_queue(label_humidity_value, "--.--");
        return;
    }
    float temperature = 0.0f, humidity = 0.0f;
    if(!aht20_read_measure(&temperature, &humidity))
    {
        printf("[AHT20] read measurement failed\n");
        ui_send_queue(label_indoor_value, "--.--");
        ui_send_queue(label_humidity_value, "--.--");
        return;
    }
    
    if(temperature == last_temperature && humidity == last_humidity)
    {
        return;
    }

    last_temperature = temperature;
    last_humidity = humidity;
    if(humidity > 85.0f || humidity < 30.0f)
    {
        xTaskNotify(hbeep_func, 1, eSetValueWithOverwrite);//通知蜂鸣器任务执行蜂鸣,且覆盖之前的通知值
        //参数分别为 任务句柄 通知值 操作方式
    }
    char tem[8];
    char hum[8];
    printf("[AHT20] Temperature: %.1f, Humidity: %.1f\n", temperature, humidity);
    snprintf(tem, sizeof(tem), "%.1f°C", temperature);
    snprintf(hum, sizeof(hum), "%.1f%%", humidity);
    ui_send_queue(label_indoor_value, tem);
    ui_send_queue(label_humidity_value, hum);
    ui_send_queue(humidity_bar, hum);
}

static void outdoor_sync(void)
{
    weather_info_t weather = { 0 };
    const char *weather_url = "https://api.seniverse.com/v3/weather/now.json?key=SUJs_gBrmck4GWVzV&location=Chengdu&language=en&unit=c";
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
    char *icon;
    if (weather.weather_code == 1 || weather.weather_code == 3 || weather.weather_code == 0 || weather.weather_code == 2 || weather.weather_code == 38) 
        icon = "sunny";

    else if (weather.weather_code == 4 || weather.weather_code == 5 ||weather.weather_code == 6 ||weather.weather_code == 7 ||weather.weather_code == 8) 
        icon = "cloudy";

    else if (weather.weather_code == 9 )//阴天
        icon = "overcast";

    else if (weather.weather_code == 10 || weather.weather_code == 11 || weather.weather_code == 12 || weather.weather_code == 13)
        icon = "rainy";
        
    else if(weather.weather_code == 14)
        icon = "rainy";
        
    else if(weather.weather_code == 15 || weather.weather_code == 16 || weather.weather_code == 17|| weather.weather_code == 18|| weather.weather_code == 19)//大雨
        icon = "rainy";
   
    else // 扬沙、龙卷风等
        icon = "--";
    char tem[8];
    snprintf(tem, sizeof(tem), "%.1f°C", weather.temperature);
    ui_send_queue(label_outdoor_value, icon);
    ui_send_queue(label_outdoor_temp_value, tem);
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
    ui_send_queue(label_wifi_info, "smartconfig...");
    if(!esp_at_smartconfig()) //立即执行一次配网模式
        ui_send_queue(label_wifi_info, "smartconfig failed");
    return;          
}

static void smartconfig_stop_task(void)//停止配网函数
{
    if(!esp_at_stop_smartconfig()) //立即执行一次停止配网
        ui_send_queue(label_wifi_info, "stop smartconfig failed");
    else    ui_send_queue(label_wifi_info, "smartconfig stopped");
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
    work_queue_send(app_job, (void*)job);
    //static void app_job仍然意味着别的翻译单元不能直接用 app_job 名称
    //（也不能直接取它的地址），只有在本文件里取地址并传出才行。
}

static void timer_rtc_cb( TimerHandle_t xTimer)
{
    sync_job job = (sync_job)pvTimerGetTimerID(xTimer);
    job();
}

void btn_clicked_event_cb(lv_event_t *e)//lvgl按钮点击事件回调函数
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED)
        return; // 不是点击事件，直接退出

    lv_obj_t* btn = lv_event_get_target(e);
    if(btn == btn_refresh_time)
    {
        ui_send_queue(label_date, "get...");
        work_queue_send(app_job, (void*)time_sntp_sync);//立即执行一次时间同步
    }
    else if(btn == btn_refresh_weather)
    {
        ui_send_queue(label_outdoor_value, "get...");
        ui_send_queue(label_outdoor_temp_value, "get...");
        work_queue_send(app_job, (void*)outdoor_sync);//立即执行一次室外环境同步
    }
    else if(btn == btn_wifi_config && clicked_flags == 0)
    {
        clicked_flags = 1;//设置标志位，进入配网模式后再次点击才会停止配网
        ui_send_queue(label_wifi_info, "smartconfig...");
        work_queue_send(app_job, (void*)smartconfig_task);//立即执行一次配网模式
    }
    else if(btn == btn_wifi_config && clicked_flags == 1)
    {
        clicked_flags = 0;//清除标志位
        work_queue_send(app_job, (void*)smartconfig_stop_task);//立即执行一次停止配网
    }
}

void app_work_init(void)
{   
    xTaskCreate(beep_func, "beep task", 128, NULL, 7, &hbeep_func);
    xTaskCreate(usart3_control, "usart3 control", 256, NULL, 5, NULL);

    time_sync_timer = xTimerCreate("time sync", pdMS_TO_TICKS(SNTP_TIME_INTERVAL), pdTRUE,time_sntp_sync, timer_cb);
    wifi_sync_timer = xTimerCreate("wifi sync", pdMS_TO_TICKS(WIFI_TIME_INTERVAL), pdTRUE, wifi_sync, timer_cb);
    rtc_sync_timer = xTimerCreate("rtv sync", pdMS_TO_TICKS(RTC_TIME_INTERVAL), pdTRUE, time_rtc_sync, timer_rtc_cb);
    inner_sync_timer = xTimerCreate("inner sync", pdMS_TO_TICKS(INNER_TIME_INTERVAL), pdTRUE, inner_sync,timer_cb);
    outdoor_sync_timer = xTimerCreate("outdoor sync", pdMS_TO_TICKS(OUTDOOR_TIME_INTERVAL), pdTRUE, outdoor_sync, timer_cb);
    work_queue_send(app_job, (void*)wifi_sync);
    work_queue_send(app_job, (void*)inner_sync);
    work_queue_send(app_job, (void*)outdoor_sync);
    work_queue_send(app_job, (void*)time_sntp_sync);//启动时立即执行一次时间同步

    xTimerStart(time_sync_timer, 0);//启动定时器 并立即启动
    xTimerStart(wifi_sync_timer, 0);
    xTimerStart(rtc_sync_timer, 0);
    xTimerStart(inner_sync_timer, 0);
    xTimerStart(outdoor_sync_timer, 0);
}
