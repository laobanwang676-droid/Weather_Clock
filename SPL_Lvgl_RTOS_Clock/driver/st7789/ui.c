#if   0
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "st7789.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

typedef enum
{
    UI_ACTION_FILL_COLOR,//0
    UI_ACTION_WRITE_STRING,
    UI_ACTION_DRAW_IMAGE,   
} ui_action_t;

typedef struct
{
    ui_action_t action;
    union//联合体，下面所有结构体共享同一块内存，以最大结构体为准，只能同时使用一个
    {
        struct
        {
            uint16_t x;
            uint16_t y;
            uint16_t width;
            uint16_t height;
            uint16_t color;
        } fill_color;
        struct
        {
            uint16_t x;
            uint16_t y;
            char str[64];
            uint16_t color;
            uint16_t bg_color;
            const font_t *font;
        } write_string;
        struct
        {
            uint16_t x;
            uint16_t y;
            const image_t *image;
        } draw_image;        
    };
} ui_message_t;

static QueueHandle_t ui_queue;//QueueHandle_t是一个指向队列控制块结构体的指针

static void ui_func(void* param)
{
    ui_message_t ui_msg;
    st7789_init();
    while(1)
    {
        xQueueReceive(ui_queue, &ui_msg, portMAX_DELAY);//从队列中接收消息，阻塞等待
    //第一个参数ui_queue 队列句柄，指向要从中接收数据的队列。
    //第二个参数&msg指向一个缓冲区，用于存放接收到的数据。
    //第三个参数portMAX_DELAY表示等待时间。如果队列为空，任务将无限期阻塞，直到有数据可用。
        switch (ui_msg.action)
        {
        case UI_ACTION_FILL_COLOR:
            st7789_fill_color(ui_msg.fill_color.x, ui_msg.fill_color.y,
                          ui_msg.fill_color.width, ui_msg.fill_color.height,
                          ui_msg.fill_color.color);
            break;
        case UI_ACTION_WRITE_STRING:
            st7789_write_string(ui_msg.write_string.x, ui_msg.write_string.y,
                           ui_msg.write_string.str,
                           ui_msg.write_string.color,
                           ui_msg.write_string.bg_color,
                           ui_msg.write_string.font);
            break;
        case UI_ACTION_DRAW_IMAGE:
            st7789_draw_image(ui_msg.draw_image.x, ui_msg.draw_image.y,
                          ui_msg.draw_image.image);
            break;
        default:
             printf("unknown ui action\r\n");
            break;
        }
    }
}

void ui_init(void)
{   
    ui_queue = xQueueCreate(16, sizeof(ui_message_t));//创建一个队列，最多容纳10个ui_message_t类型的元素
    if(ui_queue == NULL)
    {
        printf("ui queue create failed\r\n");
        return;
    }
    xTaskCreate(ui_func, "ui_task", 1024, NULL, 6, NULL);
}

void ui_fill_color(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    ui_message_t ui_msg;
    ui_msg.action = UI_ACTION_FILL_COLOR;
    ui_msg.fill_color.x = x;
    ui_msg.fill_color.y = y;
    ui_msg.fill_color.width = width;
    ui_msg.fill_color.height = height;
    ui_msg.fill_color.color = color;
    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY);//将消息发送到队列，阻塞等待
}
void ui_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font)
{   
    ui_message_t ui_msg;
    ui_msg.action = UI_ACTION_WRITE_STRING;
    ui_msg.write_string.x = x;
    ui_msg.write_string.y = y;
    if(str != NULL)    strncpy(ui_msg.write_string.str, str, sizeof(ui_msg.write_string.str) - 1);
    else               ui_msg.write_string.str[0] = '\0';
    ui_msg.write_string.color = color;
    ui_msg.write_string.bg_color = bg_color;
    ui_msg.write_string.font = font;
    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY);
}
void ui_draw_image(uint16_t x, uint16_t y, const image_t *image)
{
    ui_message_t ui_msg;
    ui_msg.action = UI_ACTION_DRAW_IMAGE;
    ui_msg.draw_image.x = x;
    ui_msg.draw_image.y = y;
    ui_msg.draw_image.image = image;
    xQueueSend(ui_queue, &ui_msg, portMAX_DELAY);
}
#endif
