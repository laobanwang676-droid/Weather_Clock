#ifndef PAGE_H
#define PAGE_H

#include "rtc.h"

void welcome_page(void);
void error_page_display(const char *msg);
void main_page(void);

void main_page_redraw_ssid(const char *ssid);
void main_page_redraw_inner_temperature(float temperature);  
void main_page_redraw_inner_humidity(float humidity);
void main_page_redraw_outdoor_city(const char *city);
void main_page_redraw_outdoor_temperature(float temperature);
void main_page_redraw_time(rtc_date_time_t *time);
void main_page_redraw_date(rtc_date_time_t *date);
void main_page_redraw_outdoor_weather_icon(const int code);
void wifi_connecting_page(void);
void wifi_init(void);
void wifi_connect(void);

#endif /* PAGE_H */
