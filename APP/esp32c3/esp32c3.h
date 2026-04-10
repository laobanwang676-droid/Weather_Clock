#ifndef ESP32C3_H
#define ESP32C3_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	char ssid[64];
	char bssid[18];
	int channel;
	int rssi;
	bool connected;
} wifi_info_t;

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
} esp_date_time_t;

void esp32c3_init(void);
bool esp_at_write_command(const char *command, uint32_t timeout);
bool esp_at_check_ready(void);
bool esp_at_wifi_init(void);
bool esp_at_wifi_connect(const char *ssid, const char *password, const char *mac);
bool esp_at_get_wifi_info(wifi_info_t *info);//判断wifi是否连接成功
bool esp_at_sntp_init(void);
bool esp_at_sntp_get_time(esp_date_time_t *date);
const char *esp_at_http_get(const char *url);
bool esp_at_smartconfig(void);//重新配网
bool esp_at_stop_smartconfig(void);//停止配网

#endif /* ESP32C3_H */
