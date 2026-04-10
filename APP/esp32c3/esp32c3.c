#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "esp32c3.h"
#include "usart3.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
/*
    ESP32 USART1 连接方式
    PA9  -> TX
    PA10 -> RX
    3.3V -> VCC
    GND  -> GND
*/

#define ESP_AT_DEBUG   0 //调试信息
#define ARRAY_SIZE(arr)   (sizeof(arr)/sizeof((arr)[0]))

typedef enum  //枚举类型几个esp32返回的指令
{
    AT_ACK_NONE,//0
    AT_ACK_OK,
    AT_ACK_ERROR,
    AT_ACK_BUSY,
    AT_ACK_READY,
} at_ack_t;

typedef struct
{
    at_ack_t ack;
    const char *str;
}ack_match_t;//指令响应匹配表

ack_match_t ack_table[] = {
    {AT_ACK_OK, "OK\r\n"},
    {AT_ACK_ERROR, "ERROR\r\n"},
    {AT_ACK_BUSY, "busy\r\n"},
    {AT_ACK_READY, "ready\r\n"},
};

extern UART_HandleTypeDef huart1;//ESP32的串口
extern UART_HandleTypeDef huart3;//USART3的串口(用于发送语音播放)
static char rxbuf[1024];//接收缓冲区
static SemaphoreHandle_t at_ack_semaphore;//定义一个信号量用于等待指令响应
static uint32_t rxlen = 0;//接收数据长度
static at_ack_t rxack;

void esp32c3_init(void)
{
    at_ack_semaphore = xSemaphoreCreateBinary();//创建一个二值信号量
    configASSERT(at_ack_semaphore != NULL);//确保信号量创建成功
    HAL_UART_Receive_IT(&huart1, (uint8_t *)rxbuf, 1);//开始接收数据，使用中断方式，每次接收一个字节
}

static void esp_at_usart_write(const char *cmd)
{
    if(HAL_UART_Transmit_DMA(&huart1, (uint8_t *)cmd, strlen(cmd)) != HAL_OK)//使用DMA方式发送数据，如果发送失败则打印错误信息
    {
        printf("Failed to send AT command\r\n");
    }
}

static at_ack_t esp_at_match_ack(const char *str)//匹配指令响应类型
{
    for(uint8_t i = 0; i < ARRAY_SIZE(ack_table); i++)
    {
        if(strcmp(str, ack_table[i].str) == 0)//比较接收到的字符串与表中的字符串
        {
            return ack_table[i].ack;//返回匹配到的指令响应类型
        }
    }
    return AT_ACK_NONE;//如果没有匹配到任何响应，返回AT_ACK_NONE
}

static at_ack_t esp_at_wait_for_ack(uint32_t timeout)
{
    if(xSemaphoreTake(at_ack_semaphore, pdMS_TO_TICKS(timeout)) == pdTRUE)//等待信号量，直到收到指令响应或超时
    {
        return rxack;//返回接收到的指令响应类型
    }
    return AT_ACK_NONE;//如果等待超时，返回AT_ACK_NONE
}

static bool esp_at_write_command(const char *command, uint32_t timeout)
{
#if ESP_AT_DEBUG
    printf("AT Command: %s\r\n", command);
#endif
    esp_at_usart_write(command);//发送AT指令
    at_ack_t ack = esp_at_wait_for_ack(timeout);//等待指令响应
#if ESP_AT_DEBUG
    printf("AT Response: %s\r\n", rxbuf);
#endif
    return (ack == AT_ACK_OK);//如果响应是OK，返回true，否则返回false
}

static bool esp_check_ready(uint32_t timeout)
{
    esp_at_usart_write("AT+RESTORE\r\n");//发送AT+RESTORE指令恢复出厂设置，确保ESP32处于初始状态
    return (esp_at_wait_for_ack(timeout) == AT_ACK_READY) ? 1 : 0;
}

bool esp_at_check_ready(void)
{
    if(!esp_at_write_command("AT\r\n", 3000))//发送AT指令检查ESP32是否准备好
        return false;
    // if(!esp_check_ready(5000))//发送AT+RESTORE指令恢复出厂设置，确保ESP32处于初始状态，并等待ready响应
    //     return false;
    return true;
}

bool esp_at_wifi_init(void)
{//发送指令 设置为station模式作为 WiFi 客户端，连接外部已有的 WiFi 热点
    if(!esp_at_write_command("AT+CWMODE=1\r\n", 2000))
        return false;
    return true;
}
//连接WiFi热点
bool esp_at_wifi_connect(const char *ssid, const char *password, const char *mac)
{
    if(ssid == NULL || password == NULL)
        return false;
    char cmd[256];
    int len = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    if(mac != NULL)
    {
        snprintf(cmd + len, sizeof(cmd) - len, "AT+CWJAP=\"%s\",\"%s\",\"%s\"\r\n", ssid, password, mac);
    }
    return esp_at_write_command(cmd, 5000);
}

//解析AT+CWSTATE?指令的响应，提取SSID
static bool parse_cwstate_response(const char *response, wifi_info_t *info)
{
//    AT+CWSTATE?
//    +CWSTATE:2,"wifi_soft"

//    OK
    response = strstr(response, "+CWSTATE:");//查找响应中的+CWSTATE: 字符串
    if(response == NULL)
        return false;
    uint8_t state;
    if(sscanf(response, "+CWSTATE: %hhu,\"%63[^\"]", &state, info->ssid) == 2)
    {
        info->connected = (state == 2);
        return true;
    }
    return false;
}

static bool parse_cwjap_response(const char *response, wifi_info_t *info)
{//    AT+CWJAP?
//    +CWJAP:"1111","1a:2b:3c:4d:5e:6f",1,-40
    response = strstr(response, "+CWJAP:");//查找响应中的+CWJAP: 字符串
    if(response == NULL)
        return false;
    //wifi名称 mac地址 信道号 信号强度
    if(sscanf(response, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d,%d", info->ssid, info->bssid, &info->channel, &info->rssi) == 4)
        return true;
    return false;
}

bool esp_at_smartconfig(void)//重新配网
{
    if(!esp_at_write_command("AT+CWMODE=1\r\n", 2000))
        return false;
    if(!esp_at_write_command("AT+CWSTARTSMART=4,0,\"1234567890123456\"\r\n", 2000))
        return false;
    return true;
}

bool esp_at_stop_smartconfig(void)//停止配网
{
    if(!esp_at_write_command("AT+CWSTOPSMART\r\n", 2000))
        return false;
    return true;
}

bool esp_at_get_wifi_info(wifi_info_t *info)//判断wifi是否连接成功
{
    if (!esp_at_write_command("AT+CWSTATE?\r\n", 2000))//发送命令查询 ESP32 设备的 Wi-Fi 状态和 Wi-Fi 信息
        return false;
    
    if (!parse_cwstate_response(rxbuf, info))//把收到的反馈指令（在rxbuf中）进行解析 保存
        return false;
    
    if (!esp_at_write_command("AT+CWJAP?\r\n", 2000))//发送命令查询与 ESP32 Station 连接的 AP 信息
        return false;
    
    if (!parse_cwjap_response(rxbuf, info))//把收到的反馈指令（在rxbuf中）进行解析 保存
        return false;
    
    return true;
}

bool esp_at_sntp_init(void)//发送获取时间指令
{
    if (!esp_at_write_command("AT+CIPSNTPCFG=1,8\r\n", 2000))
        return false;
    
    return true;
}

static uint8_t month_str_to_num(const char *month_str)
{
	const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	for (uint8_t i = 0; i < 12; i++)
	{
		if (strcmp(month_str, months[i]) == 0)
		{
			return i + 1;
		}
	}
	return 0;
}

static uint8_t week_str_to_num(const char *week_str)
{
    const char *weeks[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (uint8_t i = 0; i < 7; i++)
    {
        if (strcmp(week_str, weeks[i]) == 0)
        {
            return i + 1;
        }
    }
    return 0;
}

static bool parse_cipsntptime_response(const char *response, esp_date_time_t *date)
{
    char month_str[4];
    char week_str[4];
    response = strstr(response, "+CIPSNTPTIME:");
    if (response == NULL)
        return false;
//	AT+CIPSNTPTIME?
//	+CIPSNTPTIME:Sun Jul 27 14:07:19 2025
//	OK
    if (sscanf(response, "+CIPSNTPTIME: %3s %3s %hhu %hhu:%hhu:%hhu %hu", week_str, month_str,
         &date->day, &date->hour, &date->minute, &date->second, &date->year) == 7)
    {
        date->month = month_str_to_num(month_str);
        date->weekday = week_str_to_num(week_str);
        return (date->month != 0 && date->weekday != 0);
    }
    return false;
}

bool esp_at_sntp_get_time(esp_date_time_t *date)
{
    if (!esp_at_write_command("AT+CIPSNTPTIME?\r\n", 2000))
        return false;
    
    if (!parse_cipsntptime_response(rxbuf, date))
        return false;
    
    return true;
}

const char *esp_at_http_get(const char *url)//网络访问请求
{
//    AT+HTTPCLIENT=2,1,"https://api.seniverse.com/v3/weather/now.json?key=SfRic8Wmp-Qh3OeFk&location=WTEMH46Z5N09&language=en&unit=c",,,2
//    +HTTPCLIENT:261,{"results":[{"location":{"id":"WTEMH46Z5N09","name":"Hefei","country":"CN","path":"Hefei,Hefei,Anhui,China","timezone":"Asia/Shanghai","timezone_offset":"+08:00"},"now":{"text":"Cloudy","code":"4","temperature":"32"},"last_update":"2025-07-26T16:30:00+08:00"}]}

//    OK
    char *txbuf = rxbuf;//复用接收缓冲区作为发送缓冲区，避免额外的内存开销。
    //是基于snprintf函数会在写入数据时自动添加字符串结束符'\0'
    //使调用dma发送函数时能够正确识别字符串的结束位置，避免发送多余的数据或发生内存越界。
    snprintf(txbuf, sizeof(rxbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);//发送访问url的固定格式
    bool ret = esp_at_write_command(txbuf, 10000);  
    return ret ? rxbuf : NULL;//如果发送成功则返回接收缓冲区中的响应数据，否则返回NULL
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)//检查是否是ESP32的串口
    {
        static char *line = rxbuf;//定义一个指针指向接收缓冲区的开始位置
        if(rxlen < sizeof(rxbuf) - 2)
        {
            if(rxbuf[rxlen++] == '\n')
            {
                rxbuf[rxlen] = '\0';//将换行符替换为字符串结束符
                at_ack_t ack = esp_at_match_ack((char *)line);//匹配指令响应类型
                if(ack != AT_ACK_NONE)
                {
                    rxlen = 0;//重置接收长度，准备接收下一条指令响应
                    rxack = ack;//保存匹配到的响应类型
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;//定义一个变量用于判断是否需要切换到更高优先级的任务
                    xSemaphoreGiveFromISR(at_ack_semaphore, &xHigherPriorityTaskWoken);//从中断中释放信号量，通知等待的任务
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);//如果有更高优先级的任务被唤醒，切换到该任务
                }
                line = rxbuf + rxlen;//更新指针位置，准备接收下一行数据
            }
        }
        HAL_UART_Receive_IT(huart, (uint8_t *)rxbuf + rxlen, 1);//继续接收下一个字节
    }

    if (huart->Instance == USART3)//检查是否是USART3的串口(用于发送语音播放)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        HAL_UART_Receive(&huart3, (uint8_t *)&control_flag, 1, 0);
        xSemaphoreGiveFromISR(control_semaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

        HAL_UART_Receive_IT(huart, (uint8_t *)&control_flag, 1);//继续接收下一个字节
    }
}
