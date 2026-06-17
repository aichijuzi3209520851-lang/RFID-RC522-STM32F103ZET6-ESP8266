#include "stm32f10x.h"
#include "DebugSerial.h"

#define RX_BUF_SIZE  DEBUG_SERIAL_RX_BUF_SIZE

static volatile uint8_t RxRing[RX_BUF_SIZE];
static volatile uint16_t RxHead;
static volatile uint16_t RxTail;

void DebugSerial_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = DEBUG_SERIAL_BAUDRATE;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	RxHead = 0;
	RxTail = 0;

	USART_Cmd(USART1, ENABLE);
}

void DebugSerial_SendByte(uint8_t byte)
{
	USART_SendData(USART1, byte);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

void DebugSerial_PrintString(const char *string)
{
	while (*string != '\0')
	{
		DebugSerial_SendByte((uint8_t)*string);
		string++;
	}
}

void DebugSerial_PrintLine(const char *string)
{
	DebugSerial_PrintString(string);
	DebugSerial_PrintString("\r\n");
}

void DebugSerial_PrintHexByte(uint8_t byte)
{
	static const char hex[] = "0123456789ABCDEF";
	DebugSerial_SendByte((uint8_t)hex[(byte >> 4) & 0x0F]);
	DebugSerial_SendByte((uint8_t)hex[byte & 0x0F]);
}

void DebugSerial_PrintCardID(const uint8_t uid[4])
{
	uint8_t i;

	DebugSerial_PrintString("CARD,");
	for (i = 0; i < 4; i++)
	{
		DebugSerial_PrintHexByte(uid[i]);
	}
	DebugSerial_PrintString("\r\n");
}

uint8_t DebugSerial_ReadLine(char *buf, uint16_t bufsize)
{
	uint16_t idx = 0;

	while (RxHead != RxTail)
	{
		uint8_t byte = RxRing[RxTail];
		RxTail = (uint16_t)((RxTail + 1) % RX_BUF_SIZE);

		if (byte == '\n')
		{
			if (idx > 0 && buf[idx - 1] == '\r')
				idx--;
			buf[idx] = '\0';
			return 1;
		}

		if (idx < bufsize - 1)
			buf[idx++] = (char)byte;
	}

	return 0;
}

void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
	{
		uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
		uint16_t next = (uint16_t)((RxHead + 1) % RX_BUF_SIZE);
		if (next != RxTail)
		{
			RxRing[RxHead] = byte;
			RxHead = next;
		}
	}
}
