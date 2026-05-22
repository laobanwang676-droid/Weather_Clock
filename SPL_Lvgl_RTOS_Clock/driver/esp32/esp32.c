#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "esp32.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define ESP_AT_DEBUG   0 //调试信息
#define ARRAY_SIZE(arr)   (sizeof(arr)/sizeof((arr)[0]))

/*
    ESP32 USART1 连接方式
    PA9  -> TX
    PA10 -> RX
    3.3V -> VCC
    GND  -> GND
*/

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
    const char *string;
} at_ack_match_t; //定义结构体用于存储可能收到的指令
//声明该结构体类型数组 可能收到的指令
static const at_ack_match_t at_ack_matches[] = 
{
    {AT_ACK_OK, "OK\r\n"},
    {AT_ACK_ERROR, "ERROR\r\n"},
    {AT_ACK_BUSY, "busy p…\r\n"},
    {AT_ACK_READY, "ready\r\n"},
};
static uint32_t rxlen = 0;
static char rxbuf[1024]; //定义一个接收缓存区数组
static at_ack_t rxack;//用于存储当前收到的指令类型
static SemaphoreHandle_t at_ack_semaphore;//定义一个信号量用于等待指令响应


static bool esp_at_write_command(const char *command, uint32_t timeout);
static bool esp_at_wait_ready(uint32_t timeout);//判断有没有收到ready指令
static void esp_at_usart_write(const char *data);

static void esp32_io_init(void)
{
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;      // RX 需要上拉，TX 也用上拉没问题
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10; // PA9: TX  PA10: RX
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

static void esp32_usart_init(void)
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx|USART_Mode_Rx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;

    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//使能接收中断
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

static void esp32_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    DMA_InitStruct.DMA_Channel = DMA_Channel_4;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);//指定 DMA 要访问的外设目标地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;//指定 DMA 数据传输的方向 内存→外设
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//外设地址的自增开关
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;//指定 DMA 从内存读取数据的宽度
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;//指定 DMA 往外设写入数据的宽度
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;//传输模式 单次传输
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;//将 DMA 内部 FIFO 的 “触发传输阈值” 设为「FIFO 完全填满」
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;//DMA 从内存读取数据时，一次连续读取 8个数据（beats）
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;//DMA 向外部设备（如 SPI）发送数据时，每次仅传输 1 个数据
    DMA_Init(DMA2_Stream7, &DMA_InitStruct);
}

static void esp32_interrupt_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void esp32_init(void)//放在board.c中初始化
{  
    at_ack_semaphore = xSemaphoreCreateBinary();//创建一个二值信号量 用于等待指令响应
    configASSERT(at_ack_semaphore);//断言确保信号量创建成功
    esp32_usart_init();
    esp32_dma_init();
    esp32_interrupt_init();
    esp32_io_init();
}

static bool esp_at_wait(void)
{
    for(uint8_t i = 0; i < 2; i++)
    {
        if (esp_at_write_command("AT\r\n", 2000))
        return true;
    }
    return false;
}

bool esp_at_init(void)
{    
    if(!esp_at_wait())
        return false;
    if (!esp_at_write_command("AT+RESTORE\r\n", 2000))
        return false;
    if (!esp_at_wait_ready(5000))
        return false;    
    return true;
}

void esp_at_usart_write(const char *data) //写指令操作
{       
    uint32_t len = strlen(data);
    DMA2_Stream7->CR |= DMA_SxCR_MINC; //开启 DMA 内存地址自增
    DMA2_Stream7->M0AR = (uint32_t)data;//指定 DMA 要读取数据的内存起始地址
    DMA2_Stream7->NDTR = len;//指定 DMA 的总传输次数
    DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_TCIF7);//清除传输完成标志位 必须手动清除否则无法触发中断
    DMA_Cmd(DMA2_Stream7, ENABLE);
}

static at_ack_t match_internal_ack(const char *str)//定义了一个返回值是枚举类型的函数
{
    for (uint32_t i = 0; i < ARRAY_SIZE(at_ack_matches); i++)//数组长度
    {
        if (strcmp(str, at_ack_matches[i].string) == 0)//与具体的返回指令做对比遇到\0结束
            return at_ack_matches[i].ack;
    }
    
    return AT_ACK_NONE;
}

static at_ack_t esp_at_usart_wait_receive(uint32_t timeout)//等待接收指令匹配成功
{  
    rxlen = 0;    
    bool acked = xSemaphoreTake(at_ack_semaphore, pdMS_TO_TICKS(timeout)) == pdTRUE;//等待信号量
    return acked ? rxack : AT_ACK_NONE;//如果在timeout时间内收到信号量就返回rxack的值 否则返回0
}

static bool esp_at_wait_ready(uint32_t timeout)    //判断是否接收到ready 是返回1 否则返回0
{
    return esp_at_usart_wait_receive(timeout) == AT_ACK_READY;
}

static bool esp_at_write_command(const char *command, uint32_t timeout)
{
#if ESP_AT_DEBUG    //调试程序  为1执行if后面的代码  为0执行 endif后面的代码
    printf("[DEBUG] Send: %s\n", command);
#endif

    esp_at_usart_write(command);    //写指令
    at_ack_t ack = esp_at_usart_wait_receive(timeout);  //将比较结果的值传入ack

#if ESP_AT_DEBUG
    printf("[DEBUG] Response:\n%s\n", rxbuf);
#endif
    return ack == AT_ACK_OK;
}
    
static const char *esp_at_get_response(void)   //提供外部访问接口  调用esp_at_get_response()可以提取rxbuf首地址
{
    return rxbuf;
}

bool esp_at_wifi_init(void)
{//发送指令 设置为station模式作为 WiFi 客户端，连接外部已有的 WiFi 热点
    if(esp_at_write_command("AT+CWMODE=1\r\n", 2000))
        return true;
    return false;
}

bool esp_at_connect_wifi(const char *ssid,const char *pwd,const char *mac)
{
    if(ssid == NULL||pwd == NULL)
        return false;

    char *cmd = rxbuf;    
    int len = snprintf(cmd, sizeof(rxbuf),"AT+CWJAP=\"%s\",\"%s\"\r\n",ssid, pwd);
    if(mac != NULL)
        snprintf(cmd+len, sizeof (rxbuf)-len, "\",%s\"",mac);
    return esp_at_write_command(cmd, 5000);
}

static bool parse_cwstate_response(const char *response, esp_wifi_info_t *info)//解析收到的反馈命令
{
//    AT+CWSTATE?
//    +CWSTATE:2,"Xiaomi Mi MIX 3_5577"

//    OK
	response = strstr(response, "+CWSTATE:");
	if (response == NULL)
		return false;
	
	uint8_t state;
	if (sscanf(response, "+CWSTATE:%hhu,\"%63[^\"]", &state, info->ssid) != 2) //%hhu是uint8_t 的占位符
		return false;
	
	info->connected = (state == 2); //把状态2（已经连接上 AP，并已经获取到 IPv4 地址）存到结构体中的conneed
	
	return true;
}

static bool parse_cwjap_response(const char *response, esp_wifi_info_t *info) //解析收到的反馈命令
{
//    AT+CWJAP?
//    +CWJAP:"Xiaomi Mi MIX 3_5577","da:b5:3a:e3:2f:60",9,-48,0,1,3,0,1

//    OK
	response = strstr(response, "+CWJAP:");
	if (response == NULL)
		return false;
	
	if (sscanf(response, "+CWJAP:\"%63[^\"]\",\"%17[^\"]\",%d,%d", info->ssid, info->bssid, &info->channel, &info->rssi) != 4)
		return false;//wifi名称 mac地址 信道号 信号强度
	
	return true;
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

bool esp_at_get_wifi_info(esp_wifi_info_t *info)//判断wifi是否连接成功
{
    if (!esp_at_write_command("AT+CWSTATE?\r\n", 2000))//发送命令查询 ESP32 设备的 Wi-Fi 状态和 Wi-Fi 信息
        return false;
    
    if (!parse_cwstate_response(esp_at_get_response(), info))//把收到的反馈指令（在rxbuf中）进行解析 保存
        return false;
    
    if (!esp_at_write_command("AT+CWJAP?\r\n", 2000))//发送命令查询与 ESP32 Station 连接的 AP 信息
        return false;
    
    if (!parse_cwjap_response(esp_at_get_response(), info))//把收到的反馈指令（在rxbuf中）进行解析 保存
        return false;
    
    return true;
}

bool esp_wifi_connect_state(void)
{
    esp_wifi_info_t info;
    if(!esp_at_get_wifi_info(&info))//如果连接成功 返回结果到
        return false;
    return info.connected;

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


 static uint8_t weekday_str_to_num(const char *weekday_str)
{
	const char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
	for (uint8_t i = 0; i < 7; i++) {
		if (strcmp(weekday_str, weekdays[i]) == 0)
		{
			return i + 1;
		}
	}
	return 0;
}

static bool parse_cipsntptime_response(const char *response, esp_date_time_t *date)
{
//	AT+CIPSNTPTIME?
//	+CIPSNTPTIME:Sun Jul 27 14:07:19 2025
//	OK
	char weekday_str[8];
	char month_str[4];
	response = strstr(response, "+CIPSNTPTIME:");
	if (sscanf(response, "+CIPSNTPTIME:%3s %3s %hhu %hhu:%hhu:%hhu %hu", 
			   weekday_str, month_str, 
			   &date->day, &date->hour, &date->minute, &date->second, &date->year) != 7)
		return false;
	
	date->weekday = weekday_str_to_num(weekday_str);
	date->month = month_str_to_num(month_str);
	
	return true;
}

bool esp_at_sntp_get_time(esp_date_time_t *date)
{
    if (!esp_at_write_command("AT+CIPSNTPTIME?\r\n", 2000))
        return false;
    
    if (!parse_cipsntptime_response(esp_at_get_response(), date))
        return false;
    
    return true;
}

const char *esp_at_http_get(const char *url)//网络访问请求
{
//    AT+HTTPCLIENT=2,1,"https://api.seniverse.com/v3/weather/now.json?key=SfRic8Wmp-Qh3OeFk&location=WTEMH46Z5N09&language=en&unit=c",,,2
//    +HTTPCLIENT:261,{"results":[{"location":{"id":"WTEMH46Z5N09","name":"Hefei","country":"CN","path":"Hefei,Hefei,Anhui,China","timezone":"Asia/Shanghai","timezone_offset":"+08:00"},"now":{"text":"Cloudy","code":"4","temperature":"32"},"last_update":"2025-07-26T16:30:00+08:00"}]}

//    OK
    char *txbuf = rxbuf;
    snprintf(txbuf, sizeof(rxbuf), "AT+HTTPCLIENT=2,1,\"%s\",,,2\r\n", url);//发送访问url的固定格式
    bool ret = esp_at_write_command(txbuf, 10000);  
    return ret ? esp_at_get_response() : NULL;
}

void USART1_IRQHandler(void)
{
    static const char *line = rxbuf;
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        if(rxlen < sizeof(rxbuf) - 1)
        {
            rxbuf[rxlen++] = USART_ReceiveData(USART1);//把接收的数据放入rxbuf
            if (rxbuf[rxlen - 1] == '\n')//寻找换行符\n(因为esp32发的数据是\r\n结尾)
            {//比较函数，指令成功会返回ok\r\n
                rxbuf[rxlen] = '\0';//放入后手动加字符串结束符
                at_ack_t ack = match_internal_ack(line);
                if (ack != AT_ACK_NONE)
                {       
                    rxack = ack;//把比较结果的值传入rxack
                    BaseType_t pxHigherPriorityTaskWoken;
                    xSemaphoreGiveFromISR(at_ack_semaphore,&pxHigherPriorityTaskWoken);//在中断服务中 “释放信号量” 的专用函数/普通任务中用 xSemaphoreGive()
                    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);//参数为true才执行任务切换
                }
                line = rxbuf + rxlen;//指向下一条指令起始地址
            }
        }
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
