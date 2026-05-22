#include <stdio.h>
#include "app_work.h"
#include "esp32c3.h"
#include "page.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"

void wifi_init(void)
{   
    wifi_connecting_page();

    if(!esp_at_check_ready())//检查ESP32是否准备好
    {
        printf("[AT] init failed\n");
        goto err;
    }
    printf("[AT] inited\n");
    
    if(!esp_at_wifi_init())//wifi模式初始化
    {
        printf("[WIFI] init failed\n");
         goto err;
    }

    printf("[WIFI] inited\n");

    if(!esp_at_sntp_init())//设置中国时区
    {
        printf("[SNTP] init failed\n");
         goto err;
    }
    printf("[SNTP] inited\n");   
    return;
 err:
    error_page_display("wifi init failed\n");
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void wifi_connect(void)
{
    uint8_t i = 0;
    for(i = 0; i < 2; i++)
    {    
        if(esp_at_wifi_connect(WIFISSID, WIFIPWD, NULL))//连接WiFi热点
        {
            printf("[WIFI] connected\n");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    // error_page_display("wifi init failed/n");
    printf("[WIFI] connect failed\n");
    // while(1)
    // {
    //     ;
    // }
}

