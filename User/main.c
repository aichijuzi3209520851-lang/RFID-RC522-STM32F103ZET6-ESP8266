#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "RC522.h"
#include "ESP8266.h"
#include "DebugSerial.h"
#include <string.h>

static void uid_to_hex(const u8 uid[4], char out[9])
{
	static const char hex[] = "0123456789ABCDEF";
	u8 i;

	for (i = 0; i < 4; i++)
	{
		out[i * 2] = hex[(uid[i] >> 4) & 0x0F];
		out[i * 2 + 1] = hex[uid[i] & 0x0F];
	}
	out[8] = '\0';
}

static void show_uid_on_oled(const u8 uid[4])
{
	OLED_ShowHexNum(3, 5, uid[0], 2);
	OLED_ShowHexNum(3, 7, uid[1], 2);
	OLED_ShowHexNum(3, 9, uid[2], 2);
	OLED_ShowHexNum(3, 11, uid[3], 2);
}

int main(void)
{
	u8 uid[4];
	u8 has_card = 0;
	u8 miss_count = 0;
	u8 mqtt_ok = 0;
	char uid_hex[9];
	char rx_line[512];

	OLED_Init();
	RC522Init();
	DebugSerial_Init();
	ESP8266_Init();

	DebugSerial_PrintLine("SYS,BOOT");

	OLED_ShowString(1, 1, "RFID ATTENDANCE");
	OLED_ShowString(2, 1, "No Card       ");
	OLED_ShowString(3, 1, "UID:");
	OLED_ShowString(4, 1, "WiFi...       ");

	DebugSerial_PrintLine("WIFI,JOINING");
	if (ESP8266_ConnectWiFi())
	{
		OLED_ShowString(4, 1, "WiFi OK       ");
		DebugSerial_PrintLine("WIFI,OK");
		DebugSerial_PrintLine("MQTT,JOINING");
		if (ESP8266_ConnectThingsCloud())
		{
			mqtt_ok = 1;
			OLED_ShowString(4, 1, "MQTT OK       ");
			DebugSerial_PrintLine("MQTT,OK");
		}
		else
		{
			OLED_ShowString(4, 1, "MQTT FAIL     ");
			DebugSerial_PrintLine("MQTT,FAIL");
		}
	}
	else
	{
		OLED_ShowString(4, 1, "WiFi FAIL     ");
		DebugSerial_PrintLine("WIFI,FAIL");
	}

	while (1)
	{
		if (DebugSerial_ReadLine(rx_line, sizeof(rx_line)))
		{
			if (strncmp(rx_line, "MQTT,", 5) == 0)
			{
				const char *json = rx_line + 5;
				OLED_ShowString(4, 1, "MQTT SENDING  ");
				if (mqtt_ok && ESP8266_PublishRaw(json))
				{
					OLED_ShowString(4, 1, "MQTT PUB OK   ");
				}
				else
				{
					OLED_ShowString(4, 1, "MQTT PUB FAIL ");
					DebugSerial_PrintLine("MQTT,PUB_FAIL");
					mqtt_ok = ESP8266_ConnectThingsCloud();
				}
			}
		}

		if (RC522_ReadCardID(uid))
		{
			miss_count = 0;
			if (!has_card)
			{
				has_card = 1;
				OLED_ShowString(2, 1, "Card Found    ");
				show_uid_on_oled(uid);
				DebugSerial_PrintCardID(uid);
				uid_to_hex(uid, uid_hex);
				OLED_ShowString(4, 1, "WAIT QT RESP  ");
			}
		}
		else
		{
			if (has_card)
			{
				if (miss_count < 5)
					miss_count++;
				if (miss_count >= 5)
				{
					has_card = 0;
					miss_count = 0;
					OLED_ShowString(2, 1, "No Card       ");
					OLED_ShowString(3, 5, "        ");
					DebugSerial_PrintLine("CARD,REMOVED");
				}
			}
		}

		Delay_ms(50);
	}
}
