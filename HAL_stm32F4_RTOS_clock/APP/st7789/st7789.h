#ifndef ST7789_H
#define ST7789_H
#include <stdint.h>
#include "font.h"
#include "image.h"

#define ST7789_WIDTH 240
#define ST7789_HEIGHT 320
#define mkcolor(r,g,b)  (((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3))

void st7789_init(void);
void st7789_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void st7789_write_ascii(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, const font_t *font);
void st7789_write_chinese(uint16_t x, uint16_t y, char *ch, uint16_t color, uint16_t bg_color, const font_t *font);
void st7789_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font);
void st7789_draw_image(uint16_t x, uint16_t y, const image_t *image);
#endif /* ST7789_H */

