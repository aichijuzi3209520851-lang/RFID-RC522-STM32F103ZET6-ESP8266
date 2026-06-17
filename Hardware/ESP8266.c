#include "stm32f10x.h"
#include "Delay.h"
#include "ESP8266.h"
#include "DebugSerial.h"
#include <stdio.h>
#include <string.h>

#define ESP8266_RX_BUFFER_SIZE        512
#define ESP8266_RX_RING_SIZE          512

static char ESP8266_RxBuffer[ESP8266_RX_BUFFER_SIZE];
static uint16_t ESP8266_RxIndex;
static volatile uint8_t ESP8266_RxRing[ESP8266_RX_RING_SIZE];
static volatile uint16_t ESP8266_RxHead;
static volatile uint16_t ESP8266_RxTail;

static void ESP8266_UART4_SendByte(uint8_t byte)
{
	USART_SendData(UART4, byte);
	while (USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
}

static void ESP8266_UART4_SendString(const char *string)
{
	while (*string != '\0')
	{
		ESP8266_UART4_SendByte((uint8_t)*string);
		string++;
	}
	while (USART_GetFlagStatus(UART4, USART_FLAG_TC) == RESET);
}

static void ESP8266_UART4_SendRaw(const char *data, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++)
	{
		ESP8266_UART4_SendByte((uint8_t)data[i]);
	}
	while (USART_GetFlagStatus(UART4, USART_FLAG_TC) == RESET);
}

static uint16_t ESP8266_RingAvailable(void)
{
	return (uint16_t)((ESP8266_RxHead + ESP8266_RX_RING_SIZE - ESP8266_RxTail) % ESP8266_RX_RING_SIZE);
}

static uint8_t ESP8266_RingReadByte(void)
{
	uint8_t byte;

	if (ESP8266_RxHead == ESP8266_RxTail)
	{
		return 0;
	}

	byte = ESP8266_RxRing[ESP8266_RxTail];
	ESP8266_RxTail = (uint16_t)((ESP8266_RxTail + 1) % ESP8266_RX_RING_SIZE);
	return byte;
}

static void ESP8266_AppendResponseByte(uint8_t byte)
{
	if (ESP8266_RxIndex < ESP8266_RX_BUFFER_SIZE - 1)
	{
		ESP8266_RxBuffer[ESP8266_RxIndex++] = (char)byte;
		ESP8266_RxBuffer[ESP8266_RxIndex] = '\0';
	}
}

static void ESP8266_ReadAvailable(void)
{
	while (ESP8266_RingAvailable() > 0)
	{
		ESP8266_AppendResponseByte(ESP8266_RingReadByte());
	}
}

void ESP8266_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = ESP8266_USART_BAUDRATE;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART4, &USART_InitStructure);

	USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	ESP8266_RxHead = 0;
	ESP8266_RxTail = 0;

	USART_Cmd(UART4, ENABLE);
	ESP8266_ClearBuffer();
}

void ESP8266_ClearBuffer(void)
{
	uint16_t i;

	for (i = 0; i < ESP8266_RX_BUFFER_SIZE; i++)
	{
		ESP8266_RxBuffer[i] = '\0';
	}
	ESP8266_RxIndex = 0;
	ESP8266_RxTail = ESP8266_RxHead;
}

char *ESP8266_GetBuffer(void)
{
	return ESP8266_RxBuffer;
}

static void ESP8266_PrintResponse(void)
{
	DebugSerial_PrintString("[ESP8266] << ");
	if (ESP8266_RxBuffer[0] == '\0')
	{
		DebugSerial_PrintLine("(no response)");
	}
	else
	{
		DebugSerial_PrintLine(ESP8266_RxBuffer);
	}
}

static uint8_t ESP8266_ResponseHas(const char *ack)
{
	return (uint8_t)((ack != 0) && (strstr(ESP8266_RxBuffer, ack) != 0));
}

static uint8_t ESP8266_SendCommand2Debug(const char *command,
                                         const char *debug_command,
                                         const char *ack1,
                                         const char *ack2,
                                         uint16_t timeout_ms)
{
	ESP8266_ClearBuffer();
	DebugSerial_PrintString("[ESP8266] >> ");
	DebugSerial_PrintLine(debug_command);
	ESP8266_UART4_SendString(command);
	ESP8266_UART4_SendString("\r\n");

	while (timeout_ms--)
	{
		ESP8266_ReadAvailable();
		if (ESP8266_ResponseHas(ack1) || ESP8266_ResponseHas(ack2))
		{
			ESP8266_PrintResponse();
			DebugSerial_PrintLine("[ESP8266] result: OK");
			return 1;
		}
		if ((strstr(ESP8266_RxBuffer, "ERROR") != 0) ||
		    (strstr(ESP8266_RxBuffer, "FAIL") != 0))
		{
			ESP8266_PrintResponse();
			DebugSerial_PrintLine("[ESP8266] result: ERROR/FAIL");
			return 0;
		}
		if (strstr(ESP8266_RxBuffer, "busy") != 0)
		{
			ESP8266_ClearBuffer();
			timeout_ms += 10;
			continue;
		}
		Delay_ms(1);
	}

	ESP8266_PrintResponse();
	DebugSerial_PrintLine("[ESP8266] result: TIMEOUT");
	return 0;
}

static uint8_t ESP8266_SendCommand2(const char *command, const char *ack1, const char *ack2, uint16_t timeout_ms)
{
	return ESP8266_SendCommand2Debug(command, command, ack1, ack2, timeout_ms);
}

uint8_t ESP8266_SendCommand(const char *command, const char *ack, uint16_t timeout_ms)
{
	return ESP8266_SendCommand2(command, ack, 0, timeout_ms);
}

uint8_t ESP8266_ConnectWiFi(void)
{
	char command[96];
	uint8_t retry;

	DebugSerial_PrintLine("[ESP8266] boot wait 2000ms");
	Delay_ms(2000);

	for (retry = 0; retry < 5; retry++)
	{
		if (ESP8266_SendCommand("AT", "OK", 1000))
		{
			break;
		}
		DebugSerial_PrintLine("[ESP8266] AT retry");
		Delay_ms(500);
	}
	if (retry >= 5)
	{
		DebugSerial_PrintLine("[ESP8266] module not ready");
		return 0;
	}

	DebugSerial_PrintLine("[ESP8266] reset module");
	ESP8266_SendCommand("AT+RST", "ready", 5000);
	Delay_ms(500);
	ESP8266_ClearBuffer();

	ESP8266_SendCommand("ATE0", "OK", 1000);
	if (!ESP8266_SendCommand("AT+CWMODE=1", "OK", 1000))
	{
		DebugSerial_PrintLine("[ESP8266] set station mode failed");
		return 0;
	}

	DebugSerial_PrintLine("[ESP8266] joining WiFi SSID: 666");
	sprintf(command, "AT+CWJAP=\"%s\",\"%s\"", ESP8266_WIFI_SSID, ESP8266_WIFI_PASSWORD);
	if ((!ESP8266_SendCommand2(command, "WIFI GOT IP", "OK", 20000)) &&
	    (!ESP8266_SendCommand("AT+CIFSR", "STAIP", 3000)))
	{
		DebugSerial_PrintLine("[ESP8266] join WiFi failed");
		return 0;
	}

	DebugSerial_PrintLine("[ESP8266] WiFi connected");
	return 1;
}

uint8_t ESP8266_ConnectThingsCloud(void)
{
	char command[192];

	DebugSerial_PrintLine("[MQTT] configure ThingsCloud");
	sprintf(command,
	        "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
	        THINGSCLOUD_CLIENT_ID,
	        THINGSCLOUD_ACCESS_TOKEN,
	        THINGSCLOUD_PROJECT_KEY);
	if (!ESP8266_SendCommand2Debug(command, "AT+MQTTUSERCFG=<hidden>", "OK", 0, 3000))
	{
		DebugSerial_PrintLine("[MQTT] user config failed");
		return 0;
	}

	DebugSerial_PrintLine("[MQTT] connect ThingsCloud");
	sprintf(command,
	        "AT+MQTTCONN=0,\"%s\",%d,1",
	        THINGSCLOUD_MQTT_HOST,
	        THINGSCLOUD_MQTT_PORT);
	if (!ESP8266_SendCommand2(command, "OK", "MQTTCONNECTED", 10000))
	{
		DebugSerial_PrintLine("[MQTT] connect failed");
		return 0;
	}

	ESP8266_SendCommand("AT+MQTTSUB=0,\"attributes/response\",0", "OK", 3000);
	DebugSerial_PrintLine("[MQTT] ThingsCloud connected");
	return 1;
}

uint8_t ESP8266_PublishRaw(const char *json)
{
	char command[128];
	uint16_t len;

	len = (uint16_t)strlen(json);

	DebugSerial_PrintLine("[MQTT] publish raw attributes");

	sprintf(command, "AT+MQTTPUBRAW=0,\"attributes\",%u,0,0", len);
	if (!ESP8266_SendCommand(command, ">", 5000))
	{
		DebugSerial_PrintLine("[MQTT] publish failed (no prompt)");
		return 0;
	}

	ESP8266_ClearBuffer();
	ESP8266_UART4_SendRaw(json, len);

	if (!ESP8266_SendCommand("", "OK", 10000))
	{
		DebugSerial_PrintLine("[MQTT] publish failed (no OK)");
		return 0;
	}

	DebugSerial_PrintLine("[MQTT] publish OK");
	return 1;
}

void UART4_IRQHandler(void)
{
	if (USART_GetITStatus(UART4, USART_IT_RXNE) != RESET)
	{
		uint8_t byte;
		uint16_t next;

		byte = (uint8_t)USART_ReceiveData(UART4);
		next = (uint16_t)((ESP8266_RxHead + 1) % ESP8266_RX_RING_SIZE);
		if (next != ESP8266_RxTail)
		{
			ESP8266_RxRing[ESP8266_RxHead] = byte;
			ESP8266_RxHead = next;
		}
		USART_ClearITPendingBit(UART4, USART_IT_RXNE);
	}
}
