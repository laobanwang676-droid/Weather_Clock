#include "ui.h"
#include "page.h"
#include "image.h"
#include "font.h"

void welcome_page(void)
{   
    const uint16_t color_bg = mkcolor(0x00,0x00,0x00);//清除屏幕
    const uint16_t color_white = mkcolor(0xff,0xff,0xff);
    const uint16_t color_bule =  mkcolor(0, 80, 180);
    ui_fill_color(0,0,UI_WIDTH - 1,UI_HEIGHT - 1, color_bg);
    ui_draw_image(22,0,&welcome_image);
    ui_write_string(47, 160, "果",color_bule,color_bg,&font48_welcome);
    ui_write_string(143, 160, "冻",color_bule,color_bg,&font48_welcome);
    ui_write_string(9, 200, "天",color_bule,color_bg,&font48_welcome);
    ui_write_string(67, 200, "气",color_bule,color_bg,&font48_welcome);
    ui_write_string(124, 200, "时",color_bule,color_bg,&font48_welcome);
    ui_write_string(183, 200, "钟",color_bule,color_bg,&font48_welcome);    
    ui_write_string(20, 260, "Welcome!!!",color_white,color_bg,&font48_welcome);    
}

void wifi_connecting_page(void)
{   
    const uint16_t color_bg = mkcolor(0x00,0x00,0x00);
    const uint16_t color_white = mkcolor(0xff,0xff,0xff);
    ui_write_string(20, 260, "Welcome!!!",color_bg, color_bg, &font48_welcome);    
    ui_write_string(15, 265, "wifi connecting...", color_white, color_bg,&font24_character1);    
}
