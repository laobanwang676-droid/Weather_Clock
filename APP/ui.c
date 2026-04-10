#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"  
#include "st7789.h" 
#include <string.h>
#include "font.h"

typedef enum 
{
    fill_color_action = 0,
    write_string_action,
    draw_image_action,
}ui_action_t;

typedef struct
{
    ui_action_t action;
        union
        {
            struct 
            {
                uint16_t x1;
                uint16_t y1;
                uint16_t x2;
                uint16_t y2;
                uint16_t color;
            }fill_color;
            struct
            {
                uint16_t x;
                uint16_t y;
                char str[64];
                uint16_t color;
                uint16_t bg_color;
                const font_t *font;
            }write_string;
            struct
            {
                uint16_t x;
                uint16_t y;
                const image_t *image;
            }draw_image;
        };
}ui_message_t;

static QueueHandle_t ui_queue;

void ui_func(void *param)
{
    ui_message_t msg;
    while(1)
    {
        if(xQueueReceive(ui_queue, &msg, portMAX_DELAY) == pdPASS)
        {
            switch(msg.action)
            {
                case fill_color_action:
                    st7789_fill_color(msg.fill_color.x1, msg.fill_color.y1, msg.fill_color.x2, msg.fill_color.y2, msg.fill_color.color);
                    break;
                case write_string_action:
                    st7789_write_string(msg.write_string.x, msg.write_string.y, msg.write_string.str, msg.write_string.color, msg.write_string.bg_color, msg.write_string.font);
                    break;
                case draw_image_action:
                    st7789_draw_image(msg.draw_image.x, msg.draw_image.y, msg.draw_image.image);
                    break;
                default:
                    break;
            }
        }
    }
}

void ui_init(void)
{
    ui_queue = xQueueCreate(10, sizeof(ui_message_t));
    configASSERT(ui_queue != NULL);
    xTaskCreate(ui_func, "ui_task", 512, NULL, 6, NULL);
}

void ui_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    ui_message_t msg;
    msg.action = fill_color_action;
    msg.fill_color.x1 = x1;
    msg.fill_color.y1 = y1;
    msg.fill_color.x2 = x2;
    msg.fill_color.y2 = y2;
    msg.fill_color.color = color;
    xQueueSend(ui_queue, &msg, portMAX_DELAY);
}

void ui_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font)
{
    ui_message_t msg;
    msg.action = write_string_action;
    msg.write_string.x = x;
    msg.write_string.y = y;
    if(str != NULL)    strncpy(msg.write_string.str, str, sizeof(msg.write_string.str) - 1);
    else               msg.write_string.str[0] = '\0';
    msg.write_string.color = color;
    msg.write_string.bg_color = bg_color;
    msg.write_string.font = font;
    xQueueSend(ui_queue, &msg, portMAX_DELAY);
}

void ui_draw_image(uint16_t x, uint16_t y, const image_t *image)
{
    ui_message_t msg;
    msg.action = draw_image_action;
    msg.draw_image.x = x;
    msg.draw_image.y = y;
    msg.draw_image.image = image;
    xQueueSend(ui_queue, &msg, portMAX_DELAY);
}
