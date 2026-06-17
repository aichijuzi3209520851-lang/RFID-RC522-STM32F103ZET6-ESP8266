#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "RC522.h"
#include "Delay.h"

#define MAXRLEN 18

static void delay_10ms(unsigned int count)
{
	while (count--)
	{
		Delay_ms(10);
	}
}

char PcdRequest(unsigned char req_code, unsigned char *pTagType)
{
	char status;
	unsigned int unLen;
	unsigned char ucComMF522Buf[MAXRLEN];

	ClearBitMask(Status2Reg, 0x08);
	WriteRawRC(BitFramingReg, 0x07);
	SetBitMask(TxControlReg, 0x03);

	ucComMF522Buf[0] = req_code;
	status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 1, ucComMF522Buf, &unLen);

	if ((status == MI_OK) && (unLen == 0x10))
	{
		*pTagType = ucComMF522Buf[0];
		*(pTagType + 1) = ucComMF522Buf[1];
	}
	else
	{
		status = MI_ERR;
	}

	return status;
}

char PcdAnticoll(unsigned char *pSnr)
{
	char status;
	unsigned char i;
	unsigned char snr_check = 0;
	unsigned int unLen;
	unsigned char ucComMF522Buf[MAXRLEN];

	ClearBitMask(Status2Reg, 0x08);
	WriteRawRC(BitFramingReg, 0x00);
	ClearBitMask(CollReg, 0x80);

	ucComMF522Buf[0] = PICC_ANTICOLL1;
	ucComMF522Buf[1] = 0x20;

	status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 2, ucComMF522Buf, &unLen);

	if (status == MI_OK)
	{
		for (i = 0; i < 4; i++)
		{
			*(pSnr + i) = ucComMF522Buf[i];
			snr_check ^= ucComMF522Buf[i];
		}
		if (snr_check != ucComMF522Buf[i])
		{
			status = MI_ERR;
		}
	}

	SetBitMask(CollReg, 0x80);
	return status;
}

char PcdSelect(unsigned char *pSnr)
{
	char status;
	unsigned char i;
	unsigned int unLen;
	unsigned char ucComMF522Buf[MAXRLEN];

	ucComMF522Buf[0] = PICC_ANTICOLL1;
	ucComMF522Buf[1] = 0x70;
	ucComMF522Buf[6] = 0;
	for (i = 0; i < 4; i++)
	{
		ucComMF522Buf[i + 2] = *(pSnr + i);
		ucComMF522Buf[6] ^= *(pSnr + i);
	}
	CalulateCRC(ucComMF522Buf, 7, &ucComMF522Buf[7]);

	ClearBitMask(Status2Reg, 0x08);
	status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 9, ucComMF522Buf, &unLen);

	if (!((status == MI_OK) && (unLen == 0x18)))
	{
		status = MI_ERR;
	}

	return status;
}

char PcdAuthState(unsigned char auth_mode, unsigned char addr, unsigned char *pKey, unsigned char *pSnr)
{
	char status;
	unsigned int unLen;
	unsigned char i;
	unsigned char ucComMF522Buf[MAXRLEN];

	ucComMF522Buf[0] = auth_mode;
	ucComMF522Buf[1] = addr;
	for (i = 0; i < 6; i++)
	{
		ucComMF522Buf[i + 2] = *(pKey + i);
	}
	for (i = 0; i < 4; i++)
	{
		ucComMF522Buf[i + 8] = *(pSnr + i);
	}

	status = PcdComMF522(PCD_AUTHENT, ucComMF522Buf, 12, ucComMF522Buf, &unLen);
	if ((status != MI_OK) || (!(ReadRawRC(Status2Reg) & 0x08)))
	{
		status = MI_ERR;
	}

	return status;
}

char PcdRead(unsigned char addr, unsigned char *pData)
{
	char status;
	unsigned int unLen;
	unsigned char i;
	unsigned char ucComMF522Buf[MAXRLEN];

	ucComMF522Buf[0] = PICC_READ;
	ucComMF522Buf[1] = addr;
	CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);

	status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &unLen);
	if ((status == MI_OK) && (unLen == 0x90))
	{
		for (i = 0; i < 16; i++)
		{
			*(pData + i) = ucComMF522Buf[i];
		}
	}
	else
	{
		status = MI_ERR;
	}

	return status;
}

char PcdWrite(unsigned char addr, unsigned char *pData)
{
	char status;
	unsigned int unLen;
	unsigned char i;
	unsigned char ucComMF522Buf[MAXRLEN];

	ucComMF522Buf[0] = PICC_WRITE;
	ucComMF522Buf[1] = addr;
	CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);

	status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &unLen);

	if ((status != MI_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
	{
		status = MI_ERR;
	}

	if (status == MI_OK)
	{
		for (i = 0; i < 16; i++)
		{
			ucComMF522Buf[i] = *(pData + i);
		}
		CalulateCRC(ucComMF522Buf, 16, &ucComMF522Buf[16]);

		status = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 18, ucComMF522Buf, &unLen);
		if ((status != MI_OK) || (unLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A))
		{
			status = MI_ERR;
		}
	}

	return status;
}

char PcdHalt(void)
{
	unsigned int unLen;
	unsigned char ucComMF522Buf[MAXRLEN];

	ucComMF522Buf[0] = PICC_HALT;
	ucComMF522Buf[1] = 0;
	CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);

	PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &unLen);
	return MI_OK;
}

void CalulateCRC(unsigned char *pIndata, unsigned char len, unsigned char *pOutData)
{
	unsigned char i;
	unsigned char n;

	ClearBitMask(DivIrqReg, 0x04);
	WriteRawRC(CommandReg, PCD_IDLE);
	SetBitMask(FIFOLevelReg, 0x80);
	for (i = 0; i < len; i++)
	{
		WriteRawRC(FIFODataReg, *(pIndata + i));
	}
	WriteRawRC(CommandReg, PCD_CALCCRC);

	i = 0xFF;
	do
	{
		n = ReadRawRC(DivIrqReg);
		i--;
	}
	while ((i != 0) && !(n & 0x04));

	pOutData[0] = ReadRawRC(CRCResultRegL);
	pOutData[1] = ReadRawRC(CRCResultRegM);
}

char PcdReset(void)
{
	RST_H;
	delay_10ms(1);
	RST_L;
	delay_10ms(1);
	RST_H;
	delay_10ms(10);

	WriteRawRC(CommandReg, PCD_RESETPHASE);

	WriteRawRC(ModeReg, 0x3D);
	WriteRawRC(TReloadRegL, 30);
	WriteRawRC(TReloadRegH, 0);
	WriteRawRC(TModeReg, 0x8D);
	WriteRawRC(TPrescalerReg, 0x3E);
	WriteRawRC(TxAutoReg, 0x40);
	return MI_OK;
}

char M500PcdConfigISOType(unsigned char type)
{
	if (type != 'A')
	{
		return (char)-1;
	}

	ClearBitMask(Status2Reg, 0x08);
	WriteRawRC(ModeReg, 0x3D);
	WriteRawRC(RxSelReg, 0x86);
	WriteRawRC(RFCfgReg, 0x7F);
	WriteRawRC(TReloadRegL, 30);
	WriteRawRC(TReloadRegH, 0);
	WriteRawRC(TModeReg, 0x8D);
	WriteRawRC(TPrescalerReg, 0x3E);

	delay_10ms(1);
	PcdAntennaOn();
	return MI_OK;
}

unsigned char ReadRawRC(unsigned char Address)
{
	unsigned char i;
	unsigned char ucAddr;
	unsigned char ucResult = 0;

	NSS_L;
	ucAddr = ((Address << 1) & 0x7E) | 0x80;

	for (i = 8; i > 0; i--)
	{
		SCK_L;
		if (ucAddr & 0x80)
		{
			MOSI_H;
		}
		else
		{
			MOSI_L;
		}
		SCK_H;
		ucAddr <<= 1;
	}

	for (i = 8; i > 0; i--)
	{
		SCK_L;
		ucResult <<= 1;
		SCK_H;
		if (READ_MISO == 1)
		{
			ucResult |= 1;
		}
	}

	NSS_H;
	SCK_H;
	return ucResult;
}

void WriteRawRC(unsigned char Address, unsigned char value)
{
	unsigned char i;
	unsigned char ucAddr;

	SCK_L;
	NSS_L;
	ucAddr = ((Address << 1) & 0x7E);

	for (i = 8; i > 0; i--)
	{
		if (ucAddr & 0x80)
		{
			MOSI_H;
		}
		else
		{
			MOSI_L;
		}
		SCK_H;
		ucAddr <<= 1;
		SCK_L;
	}

	for (i = 8; i > 0; i--)
	{
		if (value & 0x80)
		{
			MOSI_H;
		}
		else
		{
			MOSI_L;
		}
		SCK_H;
		value <<= 1;
		SCK_L;
	}
	NSS_H;
	SCK_H;
}

void SetBitMask(unsigned char reg, unsigned char mask)
{
	char tmp;

	tmp = ReadRawRC(reg);
	WriteRawRC(reg, tmp | mask);
}

void ClearBitMask(unsigned char reg, unsigned char mask)
{
	char tmp;

	tmp = ReadRawRC(reg);
	WriteRawRC(reg, tmp & ~mask);
}

char PcdComMF522(unsigned char Command,
                 unsigned char *pInData,
                 unsigned char InLenByte,
                 unsigned char *pOutData,
                 unsigned int *pOutLenBit)
{
	char status = MI_ERR;
	unsigned char irqEn = 0x00;
	unsigned char waitFor = 0x00;
	unsigned char lastBits;
	unsigned char n;
	unsigned int i;

	switch (Command)
	{
		case PCD_AUTHENT:
			irqEn = 0x12;
			waitFor = 0x10;
			break;

		case PCD_TRANSCEIVE:
			irqEn = 0x77;
			waitFor = 0x30;
			break;

		default:
			break;
	}

	WriteRawRC(ComIEnReg, irqEn | 0x80);
	ClearBitMask(ComIrqReg, 0x80);
	WriteRawRC(CommandReg, PCD_IDLE);
	SetBitMask(FIFOLevelReg, 0x80);

	for (i = 0; i < InLenByte; i++)
	{
		WriteRawRC(FIFODataReg, pInData[i]);
	}
	WriteRawRC(CommandReg, Command);

	if (Command == PCD_TRANSCEIVE)
	{
		SetBitMask(BitFramingReg, 0x80);
	}

	i = 2000;
	do
	{
		n = ReadRawRC(ComIrqReg);
		i--;
	}
	while ((i != 0) && !(n & 0x01) && !(n & waitFor));

	ClearBitMask(BitFramingReg, 0x80);

	if (i != 0)
	{
		if (!(ReadRawRC(ErrorReg) & 0x1B))
		{
			status = MI_OK;
			if (n & irqEn & 0x01)
			{
				status = MI_NOTAGERR;
			}
			if (Command == PCD_TRANSCEIVE)
			{
				n = ReadRawRC(FIFOLevelReg);
				lastBits = ReadRawRC(ControlReg) & 0x07;
				if (lastBits)
				{
					*pOutLenBit = (n - 1) * 8 + lastBits;
				}
				else
				{
					*pOutLenBit = n * 8;
				}
				if (n == 0)
				{
					n = 1;
				}
				if (n > MAXRLEN)
				{
					n = MAXRLEN;
				}
				for (i = 0; i < n; i++)
				{
					pOutData[i] = ReadRawRC(FIFODataReg);
				}
			}
		}
		else
		{
			status = MI_ERR;
		}
	}

	SetBitMask(ControlReg, 0x80);
	WriteRawRC(CommandReg, PCD_IDLE);
	return status;
}

void PcdAntennaOn(void)
{
	unsigned char i;

	i = ReadRawRC(TxControlReg);
	if (!(i & 0x03))
	{
		SetBitMask(TxControlReg, 0x03);
	}
}

void PcdAntennaOff(void)
{
	ClearBitMask(TxControlReg, 0x03);
}

void WaitCardOff(void)
{
	char status;
	unsigned char TagType[2];

	while (1)
	{
		status = PcdRequest(REQ_ALL, TagType);
		if (status)
		{
			status = PcdRequest(REQ_ALL, TagType);
			if (status)
			{
				status = PcdRequest(REQ_ALL, TagType);
				if (status)
				{
					return;
				}
			}
		}
		delay_10ms(100);
	}
}

static void RC522_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(MF522_RST_CLK | MF522_MISO_CLK | MF522_MOSI_CLK | MF522_SCK_CLK | MF522_NSS_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = MF522_RST_PIN;
	GPIO_Init(MF522_RST_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = MF522_MOSI_PIN;
	GPIO_Init(MF522_MOSI_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = MF522_SCK_PIN;
	GPIO_Init(MF522_SCK_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = MF522_NSS_PIN;
	GPIO_Init(MF522_NSS_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = MF522_MISO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(MF522_MISO_PORT, &GPIO_InitStructure);

	NSS_H;
	SCK_H;
	RST_H;
}

void RC522Init(void)
{
	RC522_GPIO_Init();
	PcdReset();
	PcdAntennaOff();
	PcdAntennaOn();
	M500PcdConfigISOType('A');
}

u8 RC522_ReadCardID(u8 uid[4])
{
	u8 TagType[2];

	if (PcdRequest(REQ_ALL, TagType) != MI_OK)
	{
		return 0;
	}
	if (PcdAnticoll(uid) != MI_OK)
	{
		return 0;
	}
	if (PcdSelect(uid) != MI_OK)
	{
		return 0;
	}

	PcdHalt();
	return 1;
}

u8 RC522_ReadCardUID_NoHalt(u8 uid[4])
{
	u8 TagType[2];

	if (PcdRequest(REQ_ALL, TagType) != MI_OK) return 0;
	if (PcdAnticoll(uid) != MI_OK)             return 0;
	if (PcdSelect(uid) != MI_OK)               return 0;
	return 1;
}

u8 RC522_ReadBlock(u8 uid[4], u8 addr, u8 data[16])
{
	static const u8 default_key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	if (PcdAuthState(PICC_AUTHENT1A, addr, (u8 *)default_key, uid) != MI_OK) return 0;
	if (PcdRead(addr, data) != MI_OK)                                       return 0;
	return 1;
}

u8 RC522_WriteBlock(u8 uid[4], u8 addr, const u8 data[16])
{
	static const u8 default_key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	if (PcdAuthState(PICC_AUTHENT1A, addr, (u8 *)default_key, uid) != MI_OK) return 0;
	if (PcdWrite(addr, (u8 *)data) != MI_OK)                                return 0;
	return 1;
}
