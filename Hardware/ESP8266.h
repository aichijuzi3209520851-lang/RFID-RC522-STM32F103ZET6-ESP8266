#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

#define ESP8266_USART_BAUDRATE        115200
#define ESP8266_WIFI_SSID             "666"
#define ESP8266_WIFI_PASSWORD         "123456789"

#define THINGSCLOUD_MQTT_HOST         "sh-1-mqtt.iot-api.com"
#define THINGSCLOUD_MQTT_PORT         1883
#define THINGSCLOUD_ACCESS_TOKEN      "pbwk6nbnfm218pzy"
#define THINGSCLOUD_PROJECT_KEY       "0PWwyIOv8U"
#define THINGSCLOUD_CLIENT_ID         "rfid_stm32"

void ESP8266_Init(void);
void ESP8266_ClearBuffer(void);
char *ESP8266_GetBuffer(void);
uint8_t ESP8266_SendCommand(const char *command, const char *ack, uint16_t timeout_ms);
uint8_t ESP8266_ConnectWiFi(void);
uint8_t ESP8266_ConnectThingsCloud(void);
uint8_t ESP8266_PublishRaw(const char *json);

#endif
