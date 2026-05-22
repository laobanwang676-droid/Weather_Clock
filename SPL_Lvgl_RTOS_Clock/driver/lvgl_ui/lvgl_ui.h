#ifndef LVGL_UI_H
#define LVGL_UI_H

#include "lvgl.h"
#include "FreeRTOS.h"
#include "queue.h"

void lvgl_init(void);
void time_ui_task(void *param);
void ui_send_queue(lv_obj_t * obj, const char * data);
void btn_clicked_event_cb(lv_event_t *e);

extern QueueHandle_t ui_queue; // 声明UI更新队列句柄
extern lv_obj_t* label_wifi_info;// wifi信息标签对象
extern lv_obj_t* label_time;// 时间标签对象
extern lv_obj_t* label_date;// 日期标签对象
extern lv_obj_t* label_indoor_value;// 室内温度数值标签对象
extern lv_obj_t* label_humidity_value;// 室内湿度数值标签对象
extern lv_obj_t* label_outdoor_temp_value;// 室外温度数值标签对象
extern lv_obj_t* label_outdoor_value;// 室外天气数值标签对象
extern lv_obj_t* btn_refresh_time;// 刷新时间按钮对象
extern lv_obj_t* btn_refresh_weather;// 刷新天气按钮对象
extern lv_obj_t* btn_wifi_config;// wifi配置按钮对象
extern lv_obj_t* humidity_bar;// 湿度进度条对象
#endif /* LVGL_UI_H */

