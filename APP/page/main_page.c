#include <stdio.h>
#include "ui.h"
#include "page.h"
#include "image.h"
#include "font.h"
#include "rtc.h"

#define ui_1_1        mkcolor(170, 226, 248)
#define ui_1_2        mkcolor(160, 226, 248)
#define ui_1_3        mkcolor(150, 226, 248)

#define ui_2_1        mkcolor(135,226, 248)
#define ui_2_2        mkcolor(115,226, 248)
#define ui_2_3        mkcolor(100,226, 248)

#define ui_3_1        mkcolor(135,226, 248)
#define ui_3_2        mkcolor(115,226, 248)
#define ui_3_3        mkcolor(100,226, 248)

void main_page(void)
{  
    ui_fill_color(0,0,UI_WIDTH - 1,UI_HEIGHT - 1, mkcolor(0x00,0x00,0x00));//清除屏幕   
    
    do{//区域1
        //ui_fill_color(5, 5, 234, 164, ui_1);
        ui_fill_color(5, 5, 234, 40, ui_1_1);
        ui_fill_color(5, 41, 234, 118, ui_1_2);
        ui_fill_color(5, 118, 234, 160, ui_1_3);
        ui_draw_image(13, 10, &icon_wifi);
        ui_draw_image(43, 10, &icon_weather);
        main_page_redraw_ssid("--");
        ui_write_string(25,42,"--:--",mkcolor(10,10,10),ui_1_2,&font76_time);
        ui_write_string(35,131,"----/--/-- --",mkcolor(10,10,10),ui_1_3,&font20_date);
    }while(0);
 
    do{//区域2
        // ui_fill_color(5, 165, 119, 314, ui_2_1);
        ui_fill_color(5, 165, 117, 194, ui_2_1);
        ui_fill_color(5, 192, 117, 246, ui_2_2);
        ui_fill_color(5, 246, 117, 314, ui_2_3);
        ui_write_string(14,170,"室内环境",mkcolor(10,10,10),ui_2_1,&font24_character1);
        ui_write_string(94,200,"C",mkcolor(10,10,10),ui_2_2,&font32_fuhao);
        ui_write_string(83,191,"o",mkcolor(10,10,10),ui_2_2,&font24_character1);
        ui_write_string(94,261,"%",mkcolor(10,10,10),ui_2_3,&font32_fuhao);
        main_page_redraw_inner_temperature(999.9f);
        main_page_redraw_inner_humidity(999.9f);
    }while(0);
    
    do{//区域3
        ui_fill_color(123, 165, 234, 314, ui_3_1);
        ui_fill_color(123, 192, 234, 314, ui_3_2);
        ui_fill_color(123, 246, 234, 314, ui_3_3);
        main_page_redraw_outdoor_city("南充");
        main_page_redraw_outdoor_temperature(999.9f);
        ui_write_string(200,189,"C",mkcolor(0,0,0),ui_3_2,&font32_fuhao);
        ui_write_string(188,179,"o",mkcolor(0,0,0),ui_2_1,&font24_character1);
        main_page_redraw_outdoor_weather_icon(99);
    }while(0);
}

void main_page_redraw_ssid(const char *ssid)
{
    char str[21];//留一位给结束符\0
    snprintf(str, sizeof(str),"%20s", ssid);
    ui_write_string(70,15, str, mkcolor(10, 10, 10), ui_1_1, &font16_wifi_name);
}

void main_page_redraw_inner_temperature(float temperature)
{
    char str[3] = {'-', '-'};
    if (temperature > -10.0f && temperature <= 100.0f)
    snprintf(str, sizeof(str), "%2.0f", temperature);
    ui_write_string(23, 192, str,mkcolor(0,0,0), ui_2_2, &font54_in_temperature);
}
    
void main_page_redraw_inner_humidity(float humidity)
{
    char str[3];
    if (humidity > 0.0f && humidity <= 99.99f)
    snprintf(str, sizeof(str), "%2.0f", humidity);
    ui_write_string(18, 239, str, mkcolor(0,0,0), ui_2_3, &font64_humidity);
}

void main_page_redraw_outdoor_city(const char *city)
{
    char str[9];
    snprintf(str, sizeof(str), "%s", city);
    ui_write_string(127, 170, str, mkcolor(0,0,0), ui_2_1, &font24_character1);
}

void main_page_redraw_outdoor_temperature(float temperature)
{
    char str[3] = {'-', '-'};
    if (temperature > -10.0f && temperature <= 100.0f)
        snprintf(str, sizeof(str), "%2.0f", temperature);
        ui_write_string(135, 190, str, mkcolor(0,0,0), ui_3_2, &font54_out_temperature);
}

void main_page_redraw_time(rtc_date_time_t *time)
{
    char str[6];
    char comma = (time->second % 2 == 0) ? ':' : ' ';//实现一秒跳动一次“：”
    snprintf(str, sizeof(str), "%02u%c%02u", time->hour, comma, time->minute);
    ui_write_string(25, 42, str, mkcolor(10,10,10), ui_1_2, &font76_time);
}

void main_page_redraw_date(rtc_date_time_t *date)
{
    char str[18];
    snprintf(str, sizeof(str), "%04u/%02u/%02u 星期%s", date->year, date->month, date->day,
        date->weekday == 1 ? "一" :
        date->weekday == 2 ? "二" :
        date->weekday == 3 ? "三" :
        date->weekday == 4 ? "四" :
        date->weekday == 5 ? "五" :
        date->weekday == 6 ? "六" :
        date->weekday == 7 ? "天" : "X");
    ui_write_string(35, 131, str, mkcolor(10,10,10), ui_1_3, &font20_date);
}

void main_page_redraw_outdoor_weather_icon(const int code)
{
    static char *icon;
    
    if (code == 1 || code == 3 || code == 0 || code == 2 || code == 38) 
        icon = "晴天";

    else if (code == 4 || code == 5 ||code == 6 ||code == 7 ||code == 8) 
        icon = "多云";

    else if (code == 9 )//阴天
        icon = "阴天";

    else if (code == 10 || code == 11 || code == 12 || code == 13)
        icon = "小雨";
        
    else if(code == 14)
        icon = "中雨";
        
    else if(code == 15 || code == 16 || code == 17|| code == 18|| code == 19)//大雨
        icon = "大雨";
    else if(code == 30 || code == 31)
        icon = "雾";
    else // 扬沙、龙卷风等
        icon = "--";
    
    ui_write_string(140, 250, icon, mkcolor(0,0,0), ui_3_3, &font32_fuhao);
}
