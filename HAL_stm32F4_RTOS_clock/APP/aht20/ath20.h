#ifndef __AHT20_H__
#define __AHT20_H__
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"


bool aht20_Init(void);
bool aht20_write(uint8_t data[],  uint32_t length);
bool aht20_read(uint8_t data[],uint32_t length);
bool aht20_start_measure(void);
bool aht20_wait_measure(void);
bool aht20_read_measure(float*temperature,float*humidity);
    

#endif /* __AHT20_H__ */ 
