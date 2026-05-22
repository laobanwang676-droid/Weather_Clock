#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx.h"  
#include "FreeRTOS.h"
#include "task.h"
#include "ath20.h"
#include "main.h"
/* 
    使用I2C1与AHT20通信
    PB6 -> I2C1_SCL
    PB7 -> I2C1_SDA
    3.3V  -> VCC
    GND  -> GND
*/
extern I2C_HandleTypeDef hi2c1;//I2C 句柄

static bool aht20_is_ready(void);
//bool aht20_write(uint8_t data[],  uint32_t length);
//static bool aht20_is_ready(void);
bool aht20_Init(void)
{    
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

bool aht20_write(uint8_t data[],  uint32_t length)
{
    uint8_t addr = 0x70; // AHT20 I2C地址
    if(HAL_I2C_Master_Transmit(&hi2c1, addr, data, length, HAL_MAX_DELAY) != HAL_OK)
    {
        return false; // 传输失败
    }
    return true; // 传输成功
}


bool aht20_read(uint8_t data[],uint32_t length)
{
    uint8_t addr = 0x70; // AHT20 I2C地址
    if(HAL_I2C_Master_Receive(&hi2c1, addr, data, length, HAL_MAX_DELAY) != HAL_OK)
    {
        return false; // 接收失败
    }
    return true; // 接收成功         
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
