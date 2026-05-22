#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "stm32f4xx.h"
#include "st7789.h"
#include "font.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*
    ST7789 SPI 连接方式
    PB13 -- SCK
    PB15 -- MOSI
    PB12 -- NSS
    PC7  -- BL
    PC6  -- DC
    PB14 -- RESET
    5V  -- VCC
*/

//SCL -- PB13 SCK
//SDA -- PB15 MOSI
//CS  -- PB12 NSS
//BL  -- PC7
//DC  -- PC6 低电平时表示当前传输的是控制命令 高电平表示传输的显示命令
//RESET- PB14 拉低此引脚可触发 ST7789 硬件复位
#define delay_us(x)  cpu_delay(x)
#define delay_ms(x)  cpu_delay((x)*1000)
#define CS_PORT      GPIOB
#define CS_PIN       GPIO_PIN_12
#define DC_PORT      GPIOC
#define DC_PIN       GPIO_PIN_6
#define BL_PORT      GPIOC
#define BL_PIN       GPIO_PIN_7
#define RESET_PORT   GPIOB
#define RESET_PIN    GPIO_PIN_14

extern SPI_HandleTypeDef hspi2;//SPI 句柄
extern DMA_HandleTypeDef hdma_spi2_tx;//DMA 句柄
static SemaphoreHandle_t write_gram_semaphore;
static void st7789_init_display(void);

void st7789_init(void)
{
    write_gram_semaphore = xSemaphoreCreateBinary();
    configASSERT(write_gram_semaphore);
    __HAL_DMA_ENABLE_IT(&hdma_spi2_tx, DMA_IT_TC);//开启DMA传输完成中断
    st7789_init_display();
}

HAL_StatusTypeDef SPI2_SetDataWidth(uint8_t width)
{
  // 1. 先禁用SPI（修改宽度前必须失能，硬件要求）  
  __HAL_SPI_DISABLE(&hspi2);
  
  // 2. 清除原有DS位配置，设置新宽度
  hspi2.Instance->CR1 &= ~0x0800; // 清空DFF的bit11（控制传输宽度）
  if (width == 8) 
  {
    hspi2.Instance->CR1 |= 0x0000; // 8位宽度
  } else if (width == 16) 
  {
    hspi2.Instance->CR1 |= 0x0800; // 16位宽度
  } 
  else 
  {
    return HAL_ERROR; // 仅支持8/16位
  }
  
  // 3. 重新启用SPI
  __HAL_SPI_ENABLE(&hspi2);
  
  return HAL_OK;
}

static void st7789_write_register(uint8_t reg,uint8_t data[],uint8_t length)
{
    SPI2_SetDataWidth(8);//命令寄存器为8位宽度
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET); //选中从机
    HAL_GPIO_WritePin(DC_PORT, DC_PIN, GPIO_PIN_RESET); //指令模式
    
    HAL_SPI_Transmit(&hspi2, &reg, 1, HAL_MAX_DELAY); //发送寄存器地址

    if(length > 0)
    {
        HAL_GPIO_WritePin(DC_PORT, DC_PIN, GPIO_PIN_SET); //数据模式
        HAL_SPI_Transmit(&hspi2, data, length, HAL_MAX_DELAY); //发送数据
    }
    while(HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY); //确保 SPI 传输完成
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET); //取消选中
}

static void st7789_dma_write_gram(uint8_t data[],uint32_t length, bool single_color)
{   
    SPI2_SetDataWidth(16);//GRAM寄存器为16位宽度
    uint8_t flag = 0;
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET); //选中从机
    HAL_GPIO_WritePin(DC_PORT, DC_PIN, GPIO_PIN_SET); //数据模式
    length /= 2;//转换为16位数据长度
    do{
        uint32_t size = length < 65535 ? length : 65535;
        if(flag == 0)
        {
            flag = 1;
            __HAL_DMA_DISABLE(&hdma_spi2_tx);//先关闭DMA
            if(single_color)        hdma_spi2_tx.Instance->CR &= ~DMA_SxCR_MINC; //单色填充时关闭 DMA 内存地址自增
            else                    hdma_spi2_tx.Instance->CR |= DMA_SxCR_MINC;  //多色填充时开启 DMA 内存地址自增
        }
        HAL_SPI_Transmit_DMA(&hspi2, data, size); //启动 DMA 传输
        xSemaphoreTake(write_gram_semaphore, portMAX_DELAY);//等待
        if(!single_color)
            data += size * 2;//移动数据指针 注意每个像素占2字节
        length -= size;
    }while (length > 0);
    
    while(HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY); //确保 SPI 传输完成
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET); //取消选中
}

static void st7789_reset(void)
{//拉低RESET引脚复位
    HAL_GPIO_WritePin(RESET_PORT,RESET_PIN,GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(1));
    HAL_GPIO_WritePin(RESET_PORT,RESET_PIN,GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void st7789_set_backlight(bool on)
{//打开背光
    HAL_GPIO_WritePin(BL_PORT, BL_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
/**
 * @brief  ST7789显示屏核心初始化函数
 * @note   完成显示屏复位、睡眠退出、显示参数配置、伽马校正、显示开启等核心流程
 */
static void st7789_init_display(void)
{
    st7789_reset();
    st7789_write_register(0x11,NULL,0);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    st7789_write_register(0x36, (uint8_t[]){0x00}, 1);  // 默认
    st7789_write_register(0x3A, (uint8_t[]){0x55}, 1);
    st7789_write_register(0xB7, (uint8_t[]){0x46}, 1);
    st7789_write_register(0xBB, (uint8_t[]){0x1B}, 1);
    st7789_write_register(0xC0, (uint8_t[]){0x2C}, 1);
    st7789_write_register(0xC2, (uint8_t[]){0x01}, 1);
    st7789_write_register(0xC4, (uint8_t[]){0x20}, 1);
    st7789_write_register(0xC6, (uint8_t[]){0x0F}, 1);
    st7789_write_register(0xD0, (uint8_t[]){0xA4,0xA1}, 2);
    st7789_write_register(0xD6, (uint8_t[]){0xA1}, 1);
    st7789_write_register(0xE0, (uint8_t[]){0xF0,0x00,0x06,0x04,0x05,0x05,0x31,0x44,0x48,0x36,0x12,0x12,0x2B,0x34}, 14);
    st7789_write_register(0xE0, (uint8_t[]){0xF0,0x0B,0x0F,0x0F,0x0D,0x26,0x31,0x43,0x47,0x38,0x14,0x14,0x2C,0x32}, 14);
    st7789_write_register(0x20, NULL, 0);
    st7789_write_register(0x29, NULL, 0);
    
    st7789_fill_color(0,0,ST7789_WIDTH - 1,ST7789_HEIGHT - 1,0x0000);//默认全填充黑色   
    st7789_set_backlight(true);  //开启背光

}

static bool in_screen_range(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    // 检查左上角坐标是否超出屏幕
    if (x1 >= ST7789_WIDTH || y1 >= ST7789_HEIGHT)
        return false;
    
    // 检查右下角坐标是否超出屏幕
    if (x2 >= ST7789_WIDTH || y2 >= ST7789_HEIGHT)
        return false;
    
    // 检查坐标的有效性（x1 应小于 x2，y1 应小于 y2）
    if (x1 > x2 || y1 > y2)
        return false;
    
    // 所有检查都通过，说明矩形在屏幕内
    return true;
}
/**
 * @brief  设置ST7789的显示窗口范围，并进入显存写入模式
 * @note   1. ST7789通过0x2A/0x2B指令限定像素写入的矩形区域（X/Y轴范围）
 *         2. 发送0x2C指令后，后续SPI传输的所有数据都会直接写入该窗口的GRAM（显存）
 *         3. 坐标为16位值，需拆分为高8位+低8位逐字节传输（SPI仅支持字节级通信）
 */
static void st7789_set_range_and_prepare_gram(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    st7789_write_register(0x2A, (uint8_t[]){(x1 >> 8) & 0xff, x1 & 0xff, (x2 >> 8) & 0xff,x2 & 0xff}, 4);
    st7789_write_register(0x2B, (uint8_t[]){(y1 >> 8) & 0xff, y1 & 0xff, (y2 >> 8) & 0xff,y2 & 0xff}, 4);
    st7789_write_register(0x2C, NULL, 0);   //指令-写入显存刷屏模式
}

void st7789_fill_color(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    if (!in_screen_range(x1, y1, x2, y2))
        return;
    
    st7789_set_range_and_prepare_gram(x1, y1, x2, y2);//显示范围
    //颜色为565格式，拆分为高八位低八位
    uint32_t pixels = (x2 - x1 + 1) * (y2 - y1 + 1);//像素点计算
    st7789_dma_write_gram((uint8_t*)&color, pixels * 2, true);//DAM传输显存数据
}

//通用的点阵绘制函数。
//不关心 model 数据是哪个字符的，
//也不关心 width 和 height 是多少。
//它只负责接收一个矩形区域 (x, y, width, height)、
//一块点阵数据 (model) 和两种颜色，然后将这块点阵数据以指定的颜色绘制到 LCD 的指定位置。
static void st7789_draw_font(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *model, uint16_t color, uint16_t bg_color)
{
    uint16_t bytes_per_row = (width + 7) / 8;//计算每一行需要几个字节
    static uint8_t all_datas[76 * 76 * 2];
    uint8_t *datas_idx = all_datas;
    for (uint16_t row = 0; row < height; row++)
    {
        const uint8_t *row_data = model + row * bytes_per_row;//计算起始位
        for (uint16_t col = 0; col <width; col++)
        {
            //     - row_data[col / 8]: 找到当前列所在的那个字节
            //     - (7 - col % 8): 计算当前列在字节中的具体位位置 (从左到右)
            //     - (1 << ...): 创建一个位掩码，只有目标位是1
            //     - &: 通过与运算提取目标位的值
            uint8_t pixel = row_data[col / 8] & (1 << (7 - col % 8));
            uint16_t write_color = pixel ? color : bg_color;
            *datas_idx++ = write_color & 0xff;   //！！！先存低字节因为dma的16位传输是先传高地址的数据
            *datas_idx++ = (write_color >> 8) & 0xff; //存高字节
        }
    } 

    st7789_set_range_and_prepare_gram(x, y, x + width - 1, y + height - 1);
    st7789_dma_write_gram(all_datas, datas_idx - all_datas, false);
} 

//由于字体库存在非ascii码排列 故需要手动计算索引
static const uint8_t *ascii_get_model(const char ch, const font_t *font)
{
    uint16_t bytes_per_row = (font->size / 2 + 7) / 8;
    uint16_t all_need_bytes = font->size * bytes_per_row;
    
    if(font->ascii_map)
    {
        const char *map = font->ascii_map;
        do{
            if(*map == ch)
            {
                return font->ascii_model + (map - font->ascii_map) * all_need_bytes;
            }
        }while(*(++map) != '\0');       
    }
    else
    {
        return  font->ascii_model + (ch - ' ') * all_need_bytes;
    }
    
    return NULL;
}

//主要获取字符的宽度和高度，
//索引对应的点阵数据的起始位model，把它传入 st7789_draw_font函数
void st7789_write_ascii(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, const font_t *font)
{
    if(font==NULL)
        return;
    uint16_t fheight = font->size;
    uint16_t fwidth = fheight/2;    
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return; // 如果超出范围，则直接返回，不执行任何绘制操作
    
    if (ch < 0x20 || ch > 0x7E)
        return;
    
	const uint8_t *model = ascii_get_model(ch, font);
    
    if (model)
        
    st7789_draw_font(x, y, fwidth, fheight, model, color, bg_color);
}

void st7789_write_chinese(uint16_t x, uint16_t y, char *ch, uint16_t color, uint16_t bg_color, const font_t *font)
{
    if (ch == NULL || font == NULL)
        return;

    uint16_t fheight = font->size, fwidth = font->size;
    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
        return;
    
    const font_chinese_t *c = font->chinese;//不需要像字符那样计算。汉字可以直接传递model
    for(;c->name!=NULL;c++)
    {
        if(strcmp(c->name,ch)==0)
            break;                           
    }
    st7789_draw_font(x, y, fwidth, fheight, c->model, color, bg_color);
}

static bool is_gb2312(char ch)
{
    return ((unsigned char)ch >= 0xA1 && (unsigned char)ch <= 0xF7);
}

void st7789_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font)
{   
    while (*str)//字符串末尾为'\0'
    {   int len=(is_gb2312(*str)?2:1);
        if (len<=0)
        {
            return;
        }
           else if (len==1)
           {
                st7789_write_ascii(x,y,*str,color,bg_color,font);
                str++;
                x+=(font->size)/2;
           }
           else if (len==2)
           {    
                char ch[5];
                strncpy(ch,str,len);
                st7789_write_chinese(x,y,ch,color,bg_color,font);
                str+=len;
                x+=(font->size);
           }                      
    }   
}

void st7789_draw_image(uint16_t x, uint16_t y, const image_t *image)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT || 
        x + image->width - 1 >= ST7789_WIDTH || y + image->height - 1 >= ST7789_HEIGHT)
        return;
    
    st7789_set_range_and_prepare_gram(x, y, x + image->width - 1, y + image->height - 1);
    uint32_t size = image->width * image->height * 2;//因为一个像素点两个字节
    st7789_dma_write_gram((uint8_t *)image->data, size, false);
}

// 正确的 HAL SPI 传输完成回调（由 HAL SPI/DMA 链接最终调用）
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2)
    {
        BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(write_gram_semaphore, &pxHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
    }
}
