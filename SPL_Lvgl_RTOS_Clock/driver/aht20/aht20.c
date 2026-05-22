#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx.h"  
#include "FreeRTOS.h"
#include "task.h"
#include "tim_delay.h"

/* 
    使用I2C1与AHT20通信
    PB6 -> I2C1_SCL
    PB7 -> I2C1_SDA
    3.3V  -> VCC
    GND  -> GND
*/

static bool aht20_write(uint8_t data[], uint32_t length);
static bool aht20_read(uint8_t data[], uint32_t length);
static bool aht20_is_ready(void);
//bool aht20_write(uint8_t data[],  uint32_t length);
//static bool aht20_is_ready(void);
bool aht20_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode=GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType=GPIO_OType_OD;
    GPIO_InitStruct.GPIO_PuPd=GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed=GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_Pin=GPIO_Pin_6|GPIO_Pin_7;
    GPIO_Init(GPIOB,&GPIO_InitStruct);
    GPIO_PinAFConfig(GPIOB,GPIO_PinSource6, GPIO_AF_I2C1);
    GPIO_PinAFConfig(GPIOB,GPIO_PinSource7, GPIO_AF_I2C1);    

    I2C_InitTypeDef I2C_InitStruct;
    I2C_StructInit(&I2C_InitStruct);
    I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStruct.I2C_ClockSpeed = 100000;  // 100kHz 标准模式（或 400000 快速模式）
    I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;//
    I2C_InitStruct.I2C_OwnAddress1 = 0x00;  // 主模式下可设为 0
    I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitStruct);
    
    vTaskDelay(pdMS_TO_TICKS(40));   
    
    if (aht20_is_ready())
        return true;
    
    if (!aht20_write((uint8_t[]){0xBE, 0x08, 0x00}, 3))
        return false;
    
    for (uint32_t t = 0; t < 20; t ++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (aht20_is_ready())
            return true;
    }
    
    return false;
}

#define I2C_CHECK_EVENT(EVENT,TIMEOUT) \
    do{ \
    uint32_t timeout=TIMEOUT; \
    while (!I2C_CheckEvent(I2C1, EVENT)&&timeout>0) \
    { \
     tim_delay_us(500); \
    timeout-=10; \
    } \
    if(timeout<=0) \
    return false; \
    } \
    while(0)


bool aht20_write(uint8_t data[],  uint32_t length)
{
    I2C_AcknowledgeConfig(I2C1, ENABLE);  // 使能I2C1应答功能
    I2C_GenerateSTART(I2C1, ENABLE);     // 生成I2C起始信号
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT,1000);  // 等待主机模式选择完成
    I2C_Send7bitAddress(I2C1, 0x70, I2C_Direction_Transmitter);  // 发送aht20地址，设置为发送模式
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED,1000);  // 等待主机发送模式选择完成
    for (uint32_t i = 0; i < length; i++)  // 循环发送数据
    {
        I2C_SendData(I2C1, data[i]);  // 发送第i个数据字节
        I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_TRANSMITTING,1000);  // 等待字节发送中
    }
    I2C_GenerateSTOP(I2C1, ENABLE);  // 生成I2C停止信号
    return true;  // 返回操作成功标志
}


bool aht20_read(uint8_t data[],uint32_t length)
{           
    I2C_AcknowledgeConfig(I2C1, ENABLE);  // I2C1应答
    I2C_GenerateSTART(I2C1, ENABLE);       // 生成I2C1起始信号
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_MODE_SELECT,1000);  // 等待主机模式选择完成
    I2C_Send7bitAddress(I2C1, 0x70, I2C_Direction_Receiver);  // 发送7位从机地址设置为接收模式
    I2C_CHECK_EVENT(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 1000);  // 等待主机接收模式选择完成
    for(uint32_t i=0;i<length;i++)
    {   if(i==length -1)
        I2C_AcknowledgeConfig(I2C1 ,DISABLE);        
        I2C_CHECK_EVENT(I2C_EVENT_MASTER_BYTE_RECEIVED, 1000);  // 等待字节接收完成
        data[i] = I2C_ReceiveData(I2C1);  // 接收数据到data
        
    }    
    I2C_GenerateSTOP(I2C1, ENABLE);  // 生成I2C1停止信号
    
    return true;

}   

static bool aht20_read_status(uint8_t *status)
{
    uint8_t cmd[1]={0x71};
    if(!aht20_write(cmd,1))
        return false;
    if(!aht20_read(status,1))
        return false;
    return true ;                
}    

//static bool aht20_is_busy(void)
//{
//    uint8_t init_cmd[3] = {0xBE, 0x08, 0x00}; // 命令+两个参数
//    if (!aht20_write(init_cmd, 3)) {          // 发送3字节初始化命令
//        return false;

//}
//    return true;
//}
static bool aht20_is_busy(void)
{
    uint8_t status;
    if(!aht20_read_status(&status))
        return false;
    return ((status & 0x80)!=0);
}

static bool aht20_is_ready(void)
{
    uint8_t status;
    if(!aht20_read_status(&status))
        return false;
    return ((status & 0x08)!=0);
    
}

bool aht20_start_measure(void)
{  uint8_t send[]={0xAC,0x33,0x00};
   return aht20_write(send,3);
 
}

bool aht20_wait_measure(void)
{   for(uint8_t i=0;i<10;i++)
    {
    vTaskDelay(pdMS_TO_TICKS (100)); 
    if(!aht20_is_busy())    
        return true;
    }
    return false;
}

bool aht20_read_measure(float*temperature,float*humidity)
{
    uint8_t data[6];
    if(!aht20_read(data,6))
        return false;
    uint32_t read_humidity=((uint32_t)data[1]<<12)|
                           ((uint32_t)data[2]<<4)|
                           (uint32_t)((data[3]&0xF0)>>4);
    
    uint32_t read_temperature=(((uint32_t)data[3]&0x0F)<<16)|
                           ((uint32_t)data[4]<<8)|
                           ((uint32_t)data[5]);
    *humidity  =(float)read_humidity*100.0f/(float)0x100000;
    *temperature =(float)read_temperature*200.0f/(float)0x100000-50.0f;
    return true;
}
