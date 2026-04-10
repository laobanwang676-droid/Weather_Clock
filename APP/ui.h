#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "image.h"
#include "font.h"

#define mkcolor(r,g,b)  (((r&0xf8)<<8)|((g&0xfc)<<3)|(b>>3))
#define UI_WIDTH 240
#define UI_HEIGHT 320

void ui_init(void);
void ui_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ui_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font);
void ui_draw_image(uint16_t x, uint16_t y, const image_t *image);

#endif /* UI_H */
