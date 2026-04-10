#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>

typedef struct
{
    uint16_t width;
    uint16_t height;
    const unsigned char *data;
} image_t;

extern const image_t welcome_image;
extern const image_t wifi_error_image;
extern const image_t wifi_image;
extern const image_t icon_wenduji;
extern const image_t icon_wifi;
extern const image_t icon_duoyun;
extern const image_t icon_leizhenyu;
extern const image_t icon_qing;
extern const image_t icon_yintian;
extern const image_t icon_yueliang;
extern const image_t icon_zhongxue;
extern const image_t icon_zhongyu;
extern const image_t icon_na;
extern const image_t icon_weather;
#endif /* __IMAGE_H__ */
