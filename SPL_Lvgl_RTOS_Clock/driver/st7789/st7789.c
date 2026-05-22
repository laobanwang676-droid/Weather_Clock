#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "stm32f4xx.h"
#include "st7789.h"
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
#define CS_PIN       GPIO_Pin_12
#define DC_PORT      GPIOC
#define DC_PIN       GPIO_Pin_6
#define BL_PORT      GPIOC
#define BL_PIN       GPIO_Pin_7
#define RESET_PORT   GPIOB
#define RESET_PIN    GPIO_Pin_14

static SemaphoreHandle_t write_gram_semaphore;
static void st7789_init_display(void);

static void st7789_io_init(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_12 | GPIO_Pin_14);
    GPIO_SetBits(GPIOC, GPIO_Pin_6 | GPIO_Pin_7);
    GPIO_ResetBits(BL_PORT, BL_PIN);

    GPIO_PinAFConfig(GPIOB,GPIO_PinSource13,GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB,GPIO_PinSource15,GPIO_AF_SPI2);   

    GPIO_InitTypeDef GPIO_InitStruct;     
     //SCL -- PB13 SCK  //SDA -- PB15 MOSI
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode=GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType=GPIO_OType_PP;   
    GPIO_InitStruct.GPIO_PuPd=GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed=GPIO_Fast_Speed;//50Mhz
    GPIO_InitStruct.GPIO_Pin=GPIO_Pin_13|GPIO_Pin_15;
    GPIO_Init(GPIOB,&GPIO_InitStruct);
    //CS  -- PB12 NSS   //RESET- PB14   //DC  -- PC6   //BL  -- PC7
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed = GPIO_Fast_Speed;

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12|GPIO_Pin_14;    
    GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7;    
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_SetBits(GPIOB,GPIO_Pin_12|GPIO_Pin_14);
    GPIO_SetBits(GPIOC,GPIO_Pin_6|GPIO_Pin_7);
}

static void st7789_spi_init(void)
{
    SPI_InitTypeDef SPI_InitStruct; 
    SPI_StructInit(&SPI_InitStruct);
    SPI_InitStruct.SPI_Direction=SPI_Direction_2Lines_FullDuplex;
    SPI_InitStruct.SPI_Mode=SPI_Mode_Master;
    SPI_InitStruct.SPI_DataSize=SPI_DataSize_8b;
    SPI_InitStruct.SPI_CPHA=SPI_CPHA_1Edge;
    SPI_InitStruct.SPI_CPOL=SPI_CPOL_Low;
    SPI_InitStruct.SPI_NSS=SPI_NSS_Soft;
    SPI_InitStruct.SPI_BaudRatePrescaler=SPI_BaudRatePrescaler_4;
    SPI_InitStruct.SPI_FirstBit=SPI_FirstBit_MSB;
    SPI_Init (SPI2,&SPI_InitStruct);
    SPI_I2S_DMACmd(SPI2,SPI_I2S_DMAReq_Tx,ENABLE);//使能SPI2的DMA发送请求
    SPI_Cmd(SPI2,ENABLE);   
}

static void st7789_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    DMA_StructInit(&DMA_InitStruct);
    /* SPI2 TX on STM32F4 typically uses DMA Channel 0 on Stream4 */
    DMA_InitStruct.DMA_Channel = DMA_Channel_0;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&(SPI2->DR);//指定 DMA 要访问的外设目标地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;//指定 DMA 数据传输的方向 内存→外设
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//外设地址的自增开关
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;//指定 DMA 从内存读取数据的宽度
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;//指定 DMA 往外设写入数据的宽度
    DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;//传输模式 单次传输
    DMA_InitStruct.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
    DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;//将 DMA 内部 FIFO 的 “触发传输阈值” 设为「FIFO 完全填满」
    DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC8;//DMA 从内存读取数据时，一次连续读取 8 个数据（beats）
    DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;//DMA 向外部设备（如 SPI）发送数据时，每次仅传输 1 个数据
    DMA_ITConfig(DMA1_Stream4, DMA_IT_TC, ENABLE);//使能 发送完成中断
    DMA_Init(DMA1_Stream4, &DMA_InitStruct);
}

static void st7789_interrupt_init(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 10;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure); 
}
void st7789_init(void)
{
    write_gram_semaphore = xSemaphoreCreateBinary();
    configASSERT(write_gram_semaphore);
    st7789_spi_init();
    st7789_dma_init();
    st7789_interrupt_init();
    st7789_io_init();
    st7789_init_display();
}

static void st7789_write_register(uint8_t reg,uint8_t data[],uint8_t length)
{
    SPI_DataSizeConfig(SPI2, SPI_DataSize_8b);//配置为8位数据传输

    GPIO_WriteBit(CS_PORT ,CS_PIN,Bit_RESET);
    GPIO_WriteBit(DC_PORT,DC_PIN,Bit_RESET);
    
    SPI_SendData(SPI2,reg);                    //发送寄存器指令
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_TXE) == RESET);
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) != RESET);
    GPIO_WriteBit(DC_PORT,DC_PIN,Bit_SET);     //发送数据指令
    while(!SPI_GetFlagStatus(SPI2,SPI_FLAG_TXE));
    for(uint16_t i=0;i<length ;i++)
    {
        SPI_SendData(SPI2,data[i]);
        while(!SPI_GetFlagStatus(SPI2,SPI_FLAG_TXE));
    }
    while (SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) != RESET);
    GPIO_SetBits(CS_PORT ,CS_PIN);
}

static void st7789_dma_write_gram(uint8_t data[],uint32_t length, bool single_color)
{   
    SPI_DataSizeConfig(SPI2, SPI_DataSize_16b);//配置为16位数据传输
    GPIO_ResetBits(CS_PORT, CS_PIN);//选中从机
    GPIO_SetBits(DC_PORT, DC_PIN);//传输显示数据
    length /= 2; 

    do{
        uint32_t size = length < 65535 ? length : 65535;
        if(single_color)        DMA1_Stream4->CR &= ~DMA_SxCR_MINC;//关闭 DMA 内存地址自增
        else                    DMA1_Stream4->CR |= DMA_SxCR_MINC; //开启 DMA 内存地址自增
        DMA1_Stream4->M0AR = (uint32_t)data;//指定 DMA 要读取数据的内存起始地址
        DMA1_Stream4->NDTR = size;//指定 DMA 的总传输次数
        DMA_Cmd(DMA1_Stream4, ENABLE);
        xSemaphoreTake(write_gram_semaphore, portMAX_DELAY);//等待
        if(!single_color)
            data += size * 2;
        length -= size;
    }while (length > 0);
    
    while(SPI_GetFlagStatus(SPI2, SPI_FLAG_BSY) == SET);
    GPIO_SetBits(CS_PORT, CS_PIN);
}

static void st7789_reset(void)
{//拉低RESET引脚复位
    GPIO_WriteBit(RESET_PORT,RESET_PIN,Bit_RESET);
    vTaskDelay(pdMS_TO_TICKS(1));
    GPIO_WriteBit(RESET_PORT,RESET_PIN,Bit_SET);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void st7789_set_backlight(bool on)
{//打开背光
    GPIO_WriteBit(BL_PORT, BL_PIN, on ? Bit_SET : Bit_RESET);
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

// 支持逐像素不同颜色用于lvgl刷新显示
void st7789_fill_colors(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint16_t *colors)
{
    if (!in_screen_range(x1, y1, x2, y2))
        return;
    
    st7789_set_range_and_prepare_gram(x1, y1, x2, y2);

    uint16_t pixels = (x2 - x1 + 1) * (y2 - y1 + 1);
    // 遍历颜色数组，逐个发送像素（每个像素颜色可能不同）
    st7789_dma_write_gram((uint8_t *)colors, pixels * 2, false);
    //由lvgl调用时传入的颜色数组已经是连续的565格式数据，直接传输即可
}

//通用的点阵绘制函数。
//不关心 model 数据是哪个字符的，
//也不关心 width 和 height 是多少。
//它只负责接收一个矩形区域 (x, y, width, height)、
//一块点阵数据 (model) 和两种颜色，然后将这块点阵数据以指定的颜色绘制到 LCD 的指定位置。
//static void st7789_draw_font(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *model, uint16_t color, uint16_t bg_color)
//{
//    uint16_t bytes_per_row = (width + 7) / 8;//计算每一行需要几个字节
//    static uint8_t all_datas[76 * 76 * 2];
//    uint8_t *datas_idx = all_datas;
//    for (uint16_t row = 0; row < height; row++)
//    {
//        const uint8_t *row_data = model + row * bytes_per_row;//计算起始位
//        for (uint16_t col = 0; col <width; col++)
//        {
//            //     - row_data[col / 8]: 找到当前列所在的那个字节
//            //     - (7 - col % 8): 计算当前列在字节中的具体位位置 (从左到右)
//            //     - (1 << ...): 创建一个位掩码，只有目标位是1
//            //     - &: 通过与运算提取目标位的值
//            uint8_t pixel = row_data[col / 8] & (1 << (7 - col % 8));
//            uint16_t write_color = pixel ? color : bg_color;
//            *datas_idx++ = write_color & 0xff;   //！！！先存低字节因为dma的16位传输是先传高地址的数据
//            *datas_idx++ = (write_color >> 8) & 0xff; //存高字节
//        }
//    } 

//    st7789_set_range_and_prepare_gram(x, y, x + width - 1, y + height - 1);
//    st7789_dma_write_gram(all_datas, datas_idx - all_datas, false);
//} 

//由于字体库存在非ascii码排列 故需要手动计算索引
//static const uint8_t *ascii_get_model(const char ch, const font_t *font)
//{
//    uint16_t bytes_per_row = (font->size / 2 + 7) / 8;
//    uint16_t all_need_bytes = font->size * bytes_per_row;
//    
//    if(font->ascii_map)
//    {
//        const char *map = font->ascii_map;
//        do{
//            if(*map == ch)
//            {
//                return font->ascii_model + (map - font->ascii_map) * all_need_bytes;
//            }
//        }while(*(++map) != '\0');       
//    }
//    else
//    {
//        return  font->ascii_model + (ch - ' ') * all_need_bytes;
//    }
//    
//    return NULL;
//}

//主要获取字符的宽度和高度，
//索引对应的点阵数据的起始位model，把它传入 st7789_draw_font函数
//void st7789_write_ascii(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, const font_t *font)
//{
//    if(font==NULL)
//        return;
//    uint16_t fheight = font->size;
//    uint16_t fwidth = fheight/2;    
//    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
//        return; // 如果超出范围，则直接返回，不执行任何绘制操作
//    
//    if (ch < 0x20 || ch > 0x7E)
//        return;
//    
//	const uint8_t *model = ascii_get_model(ch, font);
//    
//    if (model)
//        
//    st7789_draw_font(x, y, fwidth, fheight, model, color, bg_color);
//}

//void st7789_write_chinese(uint16_t x, uint16_t y, char *ch, uint16_t color, uint16_t bg_color, const font_t *font)
//{
//    if (ch == NULL || font == NULL)
//        return;

//    uint16_t fheight = font->size, fwidth = font->size;
//    if (!in_screen_range(x, y, x + fwidth - 1, y + fheight - 1))
//        return;
//    
//    const font_chinese_t *c = font->chinese;//不需要像字符那样计算。汉字可以直接传递model
//    for(;c->name!=NULL;c++)
//    {
//        if(strcmp(c->name,ch)==0)
//            break;                           
//    }
//    st7789_draw_font(x, y, fwidth, fheight, c->model, color, bg_color);
//}

//static bool is_gb2312(char ch)
//{
//    return ((unsigned char)ch >= 0xA1 && (unsigned char)ch <= 0xF7);
//}

//void st7789_write_string(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bg_color, const font_t *font)
//{   
//    while (*str)//字符串末尾为'\0'
//    {   int len=(is_gb2312(*str)?2:1);
//        if (len<=0)
//        {
//            return;
//        }
//           else if (len==1)
//           {
//                st7789_write_ascii(x,y,*str,color,bg_color,font);
//                str++;
//                x+=(font->size)/2;
//           }
//           else if (len==2)
//           {    
//                char ch[5];
//                strncpy(ch,str,len);
//                st7789_write_chinese(x,y,ch,color,bg_color,font);
//                str+=len;
//                x+=(font->size);
//           }                      
//    }   
//}

//void st7789_draw_image(uint16_t x, uint16_t y, const image_t *image)
//{
//    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT || 
//        x + image->width - 1 >= ST7789_WIDTH || y + image->height - 1 >= ST7789_HEIGHT)
//        return;
//    
//    st7789_set_range_and_prepare_gram(x, y, x + image->width - 1, y + image->height - 1);
//    uint32_t size = image->width * image->height * 2;//因为一个像素点两个字节
//    st7789_dma_write_gram((uint8_t *)image->data, size, false);
//}

void DMA1_Stream4_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA1_Stream4, DMA_IT_TCIF4) == SET)//检查是否是传输完成中断
    {
        BaseType_t pxHigherPriorityTaskWoken;//一个布尔值（为了兼容性 不然为bool） 是一个状态标志

        //函数作用：释放信号量到write_async_semaphore。并判断是否有“更高”优先级在等待这个信号量
        //如果有就把pxHigherPriorityTaskWoken置为true
        xSemaphoreGiveFromISR(write_gram_semaphore,&pxHigherPriorityTaskWoken);//在中断服务中 “释放信号量” 的专用函数/普通任务中用 xSemaphoreGive()
        portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);//参数为true才执行任务切换
        DMA_ClearITPendingBit(DMA1_Stream4, DMA_IT_TCIF4);
    }
}
