#ifndef __FONT_H__
#define __FONT_H__

#include <stdint.h>

typedef struct
{
    const char *name;
    const uint8_t *model;
} font_chinese_t;

typedef struct
{
    uint16_t size;
    const uint8_t *ascii_model;
    const char *ascii_map;
    const font_chinese_t *chinese;
} font_t;

extern const font_t font48_welcome;
extern const font_t font16_wifi_name;
extern const font_t font76_time;
extern const font_t font20_date;
extern const font_t font32_fuhao;
extern const font_t font54_in_temperature;
extern const font_t font64_humidity;
extern const font_t font54_out_temperature;
extern const font_t font24_character1;
extern const font_t font24_character2;


#endif /* __FONT_H__ */
