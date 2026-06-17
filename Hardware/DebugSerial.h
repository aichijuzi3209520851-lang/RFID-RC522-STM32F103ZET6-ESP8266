#ifndef __DEBUG_SERIAL_H
#define __DEBUG_SERIAL_H

#include "stm32f10x.h"

#define DEBUG_SERIAL_BAUDRATE        115200
#define DEBUG_SERIAL_RX_BUF_SIZE     512

void DebugSerial_Init(void);
void DebugSerial_SendByte(uint8_t byte);
void DebugSerial_PrintString(const char *string);
void DebugSerial_PrintLine(const char *string);
void DebugSerial_PrintHexByte(uint8_t byte);
void DebugSerial_PrintCardID(const uint8_t uid[4]);
uint8_t DebugSerial_ReadLine(char *buf, uint16_t bufsize);

#endif
