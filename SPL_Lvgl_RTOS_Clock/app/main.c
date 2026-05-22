#include <stdio.h> 
#include "esp32.h"
#include "weather.h"
#include "app.h"
#include "work_queue.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ui.h"
#include "lvgl_ui.h"
#include "usart3.h"

/*timer 8 > lvgl_ui 7 = been 7 > work 6 > main 5 = usart3_control 5*/
//wifi名称密码宏定义在app.h中
static void main_init(void *param);
static void wifi_init(void);
static void wifi_connect(void);
int main(void)
{   
    board_rcc_init();
    work_queue_init();
    xTaskCreate(main_init, "main_init", 256, NULL, 5, NULL);//分配1kb
    vTaskStartScheduler();
    while(1)
    {
        ;
    }
}

static void main_init(void *param)
{
    board_init();
    lvgl_init();
    xTaskCreate(time_ui_task, "ui_time_screen_create", 1024, NULL, 7, NULL);//分配4kb
    wifi_init();
    wifi_connect();
    app_work_init();//启动了“timer任务”和“蜂鸣器任务” 由于想让界面先显示出来所以放在后面启动
    vTaskDelete(NULL);//删除自己这个任务，释放内存资源
}

static void wifi_init(void)
{   
    if (!esp_at_init())//esp32初始化
    {
        printf("[AT] init failed\n");
        return;
    }
    if (!esp_at_wifi_init())//wifi模式初始化
    {
        printf("[WIFI] init failed\n");
        return;
    }
    if(!esp_at_sntp_init())//设置中国时区
    {
        printf("[SNTP] init failed\n");
        return;
    }
    printf("[AT] inited\n");
    printf("[WIFI] inited\n");
    printf("[SNTP] inited\n");   
    return;
}

static void wifi_connect(void)
{   
    uint8_t count = 0;
    while(count < 2)//尝试连接两次
    {
        if (!esp_at_connect_wifi( WIFISSID, WIFIPWD, NULL))     count++; 
        else    return;
    }
    printf("[WIFI] connect failed\n");
}
