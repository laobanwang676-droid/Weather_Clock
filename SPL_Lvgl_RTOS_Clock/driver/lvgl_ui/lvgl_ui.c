#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_font.h"   
#include "lvgl_ui.h"
#include "rtc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "work_queue.h"

QueueHandle_t ui_queue; // 定义一个队列句柄，用于UI任务与其他任务之间的通信
typedef struct {
    lv_obj_t* object; // 需要更新的对象
    char data[64];     // 需要更新的数据，假设最大长度为32
} ui_update_t;

lv_obj_t* label_wifi_info;// wifi信息标签对象

lv_obj_t* label_time;// 时间标签对象
lv_obj_t* label_date;// 日期标签对象

lv_obj_t* label_indoor_value;// 室内温度数值标签对象
lv_obj_t* label_humidity_value;// 室内湿度数值标签对象

lv_obj_t* label_outdoor_temp_value;// 室外温度数值标签对象
lv_obj_t* label_outdoor_value;// 室外天气数值标签对象

lv_obj_t* humidity_bar;// 湿度进度条对象
lv_obj_t* btn_refresh_time;// 刷新时间按钮对象
lv_obj_t* btn_refresh_weather;// 刷新天气按钮对象
lv_obj_t* btn_wifi_config;// wifi配置按钮对象

void lvgl_init(void)
{
    ui_queue = xQueueCreate(10, sizeof(ui_update_t)); // 创建一个长度为10的队列，每个元素大小为ui_update_t
    configASSERT(ui_queue); // 断言队列创建成功
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
}

/* 创建时间ui界面 */
static void ui_time_screen_create(void)
{
    lv_obj_t* scr = lv_scr_act();

    /* 1. wifi图标信息 */
    lv_obj_t* wifi_icon = lv_label_create(scr);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_icon, LV_ALIGN_TOP_LEFT, 20, 10); // 对齐到屏幕顶部左侧，向右偏移20像素，向下偏移10像素 
    label_wifi_info = lv_label_create(scr);
    lv_label_set_text(label_wifi_info, "----");
    lv_obj_set_style_text_font(label_wifi_info, &lv_font_montserrat_14, 0);
    lv_obj_align(label_wifi_info, LV_ALIGN_TOP_RIGHT, -20, 10); // 对齐到屏幕顶部右侧，向左偏移20像素，向下偏移10像素
    /* 2. 时间显示 (上方, 大字体) */
    label_time = lv_label_create(scr);
    lv_label_set_text(label_time, "00:00");
    lv_obj_set_style_text_font(label_time, &dejavu_sans_mono_48, 0);//等宽字体
    lv_obj_align(label_time, LV_ALIGN_TOP_MID, 0, 40); // 对齐到屏幕顶部中央，向下偏移40像素
    /* 3. 日期显示 (时间下方, 小字体) */
    label_date = lv_label_create(scr);
    lv_label_set_text(label_date, "0000-00-00 --");
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_align_to(label_date, label_time, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 7); // 对齐到时间标签的下方，水平左对齐，向下偏移10像素
    /* 4. 环境变量容器 (屏幕底部向上延申）用于存放室内温湿度和室外环境 */
    lv_obj_t* env_container = lv_obj_create(scr);
    lv_obj_set_size(env_container, 220, 150);// 设置容器大小为220x150像素
    lv_obj_align(env_container, LV_ALIGN_BOTTOM_MID, 0, -20); // 对齐到屏幕底部中央，向上偏移20像素
    lv_obj_set_style_border_width(env_container, 2, 0); // 设置边框线宽度
    lv_obj_set_style_radius(env_container, 10, 0); // 设置圆角半径
    lv_obj_set_style_bg_color(env_container, lv_color_hex(0xBCD4E6), 0); // 设置背景颜色为淡蓝色
    lv_obj_set_style_border_color(env_container, lv_color_hex(0x000000), 0); // 设置边框颜色为黑色
    lv_obj_set_style_pad_all(env_container, 15, 0); // 设置内边距
    // lv_obj_set_layout(env_container, LV_LAYOUT_FLEX);// 设置布局为Flex布局:弹性布局
    // lv_obj_set_flex_flow(env_container, LV_FLEX_FLOW_ROW);//设置子元素排列方向为水平
    // lv_obj_set_flex_align(env_container, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // 设置子元素在主轴方向上的对齐方式为"空间平均分布"，在交叉轴（垂直）方向上的对齐方式为"居中"
    /* 5.  三个刷新按钮设置 */
    // 刷新时间按钮
    btn_refresh_time = lv_btn_create(scr);
    lv_obj_set_size(btn_refresh_time, 60, 30);
    lv_obj_set_style_bg_color(btn_refresh_time, lv_color_hex(0x0000CC), 0);// 设置按钮背景颜色为蓝色
    lv_obj_align_to(btn_refresh_time, env_container, LV_ALIGN_OUT_TOP_LEFT, 0, -10);// 对齐到环境容器的上方，水平左对齐，向上偏移10像素
    lv_obj_t* label_btn_time = lv_label_create(btn_refresh_time);
    lv_label_set_text(label_btn_time, "get tim");
    lv_obj_center(label_btn_time);
    lv_obj_add_event_cb(btn_refresh_time, btn_clicked_event_cb, LV_EVENT_CLICKED, NULL);
    // 刷新天气按钮
    btn_refresh_weather = lv_btn_create(scr);
    lv_obj_set_size(btn_refresh_weather, 60, 30);
    lv_obj_set_style_bg_color(btn_refresh_weather, lv_color_hex(0xCC8400), 0); // 设置按钮背景颜色为珊瑚色
    lv_obj_align_to(btn_refresh_weather, env_container, LV_ALIGN_OUT_TOP_MID, 0, -10);// 对齐到环境容器的上方，水平居中对齐，向上偏移10像素
    lv_obj_t* label_btn_weather = lv_label_create(btn_refresh_weather);
    lv_label_set_text(label_btn_weather, "get wea");
    lv_obj_center(label_btn_weather);
    lv_obj_add_event_cb(btn_refresh_weather, btn_clicked_event_cb, LV_EVENT_CLICKED, NULL);
    // wifi配置按钮
    btn_wifi_config = lv_btn_create(scr);
    lv_obj_set_size(btn_wifi_config, 60, 30);
    lv_obj_set_style_bg_color(btn_wifi_config, lv_color_hex(0x4C0099), 0); // 设置按钮背景颜色为紫色
    lv_obj_align_to(btn_wifi_config, env_container, LV_ALIGN_OUT_TOP_RIGHT, 0, -10);// 对齐到环境容器的上方，水平右对齐，向上偏移10像素
    lv_obj_t* label_btn_wifi = lv_label_create(btn_wifi_config);
    lv_label_set_text(label_btn_wifi, "wifi cfi");
    lv_obj_center(label_btn_wifi);
    lv_obj_add_event_cb(btn_wifi_config, btn_clicked_event_cb, LV_EVENT_CLICKED, NULL);
    /* 5. 室内环境显示*/
    //1室内温度，字体和数值垂直排列，位于环境容器的左上方
    lv_obj_t* label_indoor_text = lv_label_create(env_container);// 创建"室内温度"标签
    lv_label_set_text(label_indoor_text, "In_tem");
    lv_obj_set_style_text_font(label_indoor_text, &lv_font_montserrat_16, 0);
    lv_obj_align(label_indoor_text, LV_ALIGN_TOP_LEFT, 0, 0);// 对齐到环境容器的左上角
    label_indoor_value = lv_label_create(env_container);// 创建"温度数值"标签
    lv_label_set_text(label_indoor_value, "--°C");
    lv_obj_set_style_text_font(label_indoor_value, &lv_font_montserrat_20, 0);
    lv_obj_align_to(label_indoor_value, label_indoor_text, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);// 对齐到"室内温度"左下方，水平左对齐
    //2创建"室内湿度"标签
    lv_obj_t* label_humidity_text = lv_label_create(env_container);// 创建"室内湿度"标签
    lv_label_set_text(label_humidity_text, "In_hum");
    lv_obj_set_style_text_font(label_humidity_text, &lv_font_montserrat_16, 0);
    lv_obj_align(label_humidity_text, LV_ALIGN_BOTTOM_LEFT, 0, -30);// 对齐到环境容器的左下角，向上偏移30像素
    /* 6. 使用进度条显示湿度数值，范围0-100%，位于"室内湿度"标签的下方 */
    humidity_bar = lv_bar_create(env_container);
    lv_obj_set_size(humidity_bar, 100, 20);
    lv_bar_set_range(humidity_bar, 0, 100);
    lv_bar_set_value(humidity_bar, 0, LV_ANIM_ON);
    lv_obj_align_to(humidity_bar, label_humidity_text, LV_ALIGN_OUT_BOTTOM_LEFT, -8, 0);// 对齐到"室内湿度"左下方，水平左对齐

    label_humidity_value = lv_label_create(env_container);// 创建"湿度数值"标签
    lv_label_set_text(label_humidity_value, "--%");
    lv_obj_set_style_text_font(label_humidity_value, &lv_font_montserrat_16, 0);
    lv_obj_align_to(label_humidity_value, humidity_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);// 对齐到湿度进度条的下方，水平左对齐
    
    /* 7. 室外环境显示*/
    //1室外天气，字体和数值垂直排列，位于环境容器的右上方
    lv_obj_t* label_outdoor_text = lv_label_create(env_container);// 创建"室外天气"标签
    lv_label_set_text(label_outdoor_text, "Out_wea");
    lv_obj_set_style_text_font(label_outdoor_text, &lv_font_montserrat_16, 0);
    lv_obj_align(label_outdoor_text, LV_ALIGN_TOP_RIGHT, 0, 0);// 对齐到环境容器的右上角
    label_outdoor_value = lv_label_create(env_container);// 创建"天气数值"标签
    lv_label_set_text(label_outdoor_value, "----");
    lv_obj_set_style_text_font(label_outdoor_value, &lv_font_montserrat_20, 0);
    lv_obj_align_to(label_outdoor_value, label_outdoor_text, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);// 对齐到"室外天气"左下方，水平左对齐
    //2创建"室外温度"标签，字体和数值垂直排列，位于环境容器的右下方
    lv_obj_t* label_outdoor_temp_text = lv_label_create(env_container);// 创建"室外温度"标签
    lv_label_set_text(label_outdoor_temp_text, "Out_tem");
    lv_obj_set_style_text_font(label_outdoor_temp_text, &lv_font_montserrat_16, 0);
    lv_obj_align(label_outdoor_temp_text, LV_ALIGN_BOTTOM_RIGHT, 0, -30);// 对齐到环境容器的右下角，向上偏移30像素
    label_outdoor_temp_value = lv_label_create(env_container);// 创建"室外温度数值"标签
    lv_label_set_text(label_outdoor_temp_value, "--°C");
    lv_obj_set_style_text_font(label_outdoor_temp_value, &lv_font_montserrat_20, 0);
    lv_obj_align_to(label_outdoor_temp_value, label_outdoor_temp_text, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);// 对齐到"室外温度"左下方，水平左对齐
    /*8. 分割线设置*/
    lv_obj_t* line1 = lv_obj_create(env_container);// 创建分割线对象
    lv_obj_set_size(line1, 150, 2);// 设置分割线大小为220x2像素
    lv_obj_set_style_border_color(line1, lv_color_hex(0x000000), 0);// 设置分割线边框颜色为黑色
    lv_obj_align(line1, LV_ALIGN_CENTER, 0, 0);// 将分割线对齐到环境容器的中心位置
    lv_obj_t* line2 = lv_obj_create(env_container);// 创建第二条分割线对象
    lv_obj_set_size(line2, 2, 100);// 设置分割线大小为2x150像素
    lv_obj_set_style_border_color(line2, lv_color_hex(0x000000), 0);// 设置分割线边框颜色为黑色
    lv_obj_align(line2, LV_ALIGN_CENTER, 0, 0);// 将分割线对齐到环境容器的中心位置
}

void ui_send_queue(lv_obj_t* obj, const char* data)
{
    ui_update_t ui_update;
    ui_update.object = obj;
    if(data != NULL)    strncpy(ui_update.data, data, sizeof(ui_update.data) - 1);
    else                ui_update.data[0] = '\0';
    xQueueSend(ui_queue, &ui_update, portMAX_DELAY); // 将更新信息发送到队列，等待时间为无限长
}

void time_ui_task(void *param)
{
    ui_update_t ui_update;
    ui_time_screen_create();// 创建时间界面
    while (1) 
    {
        if(xQueueReceive(ui_queue, &ui_update, pdMS_TO_TICKS(10)) == pdPASS) // 从队列中接收UI更新信息，等待10ms
        {   
            if(ui_update.object == humidity_bar)
            {
                int humidity_value = atoi(ui_update.data);
                lv_bar_set_value(humidity_bar, humidity_value, LV_ANIM_ON);
            }
            else     lv_label_set_text(ui_update.object, ui_update.data); // 更新UI对象的文本
        }
        lv_timer_handler(); // 处理LVGL定时器
    }
}
