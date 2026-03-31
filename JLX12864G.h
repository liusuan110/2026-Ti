#ifndef __JLX_128_64_G_H__
#define __JLX_128_64_G_H__

#include <stdint.h>
#include <msp430g2553.h>
#include "Clock.h"
#include "GPIO.h"

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t

// ���Ŷ���
#define LCD_ROM_IN_PIN  P2_3
#define LCD_ROM_OUT_PIN P2_4
#define LCD_ROM_SCK_PIN P2_5
#define LCD_ROM_CS_PIN  P1_6
#define LCD_SCLK_PIN    P1_4
#define LCD_SDA_PIN     P1_5
#define LCD_RS_PIN      P2_0
#define LCD_RST_PIN     P2_1
#define LCD_CS_PIN      P2_2

// �ֿ����Ų����꺯��
#define LCD_ROM_IN_SET()  GPIO_pinWrite(LCD_ROM_IN_PIN, HIGH)
#define LCD_ROM_IN_CLR()  GPIO_pinWrite(LCD_ROM_IN_PIN, LOW)
#define LCD_ROM_OUT_VAL() (GPIO_pinRead(LCD_ROM_OUT_PIN) != LOW)
#define LCD_ROM_SCK_SET() GPIO_pinWrite(LCD_ROM_SCK_PIN, HIGH)
#define LCD_ROM_SCK_CLR() GPIO_pinWrite(LCD_ROM_SCK_PIN, LOW)
#define LCD_ROM_CS_SET()  GPIO_pinWrite(LCD_ROM_CS_PIN, HIGH)
#define LCD_ROM_CS_CLR()  GPIO_pinWrite(LCD_ROM_CS_PIN, LOW)

// LCD���Ų����꺯��
#define LCD_SCLK_SET() GPIO_pinWrite(LCD_SCLK_PIN, HIGH)
#define LCD_SCLK_CLR() GPIO_pinWrite(LCD_SCLK_PIN, LOW)
#define LCD_SDA_SET()  GPIO_pinWrite(LCD_SDA_PIN, HIGH)
#define LCD_SDA_CLR()  GPIO_pinWrite(LCD_SDA_PIN, LOW)
#define LCD_RS_SET()   GPIO_pinWrite(LCD_RS_PIN, HIGH)
#define LCD_RS_CLR()   GPIO_pinWrite(LCD_RS_PIN, LOW)
#define LCD_RST_SET()  GPIO_pinWrite(LCD_RST_PIN, HIGH)
#define LCD_RST_CLR()  GPIO_pinWrite(LCD_RST_PIN, LOW)
#define LCD_CS_SET()   GPIO_pinWrite(LCD_CS_PIN, HIGH)
#define LCD_CS_CLR()   GPIO_pinWrite(LCD_CS_PIN, LOW)

// ������ʱ(> t * 62.5ns @16MHz��Ƶ)
#define LCD_DELAY(t) __delay_cycles(t)

void LCD_init();
void LCD_writeCmd(u8 cmd);
void LCD_writeData(u8 data);
void LCD_clear();
void LCD_setAddr(u8 page, u8 column);
void LCD_showGB2312Str(u8 page, u8 column, u8* text);
void LCD_getAndWrite16x16(u32 fontAddr, u8 page, u8 column);
void LCD_getAndWrite8x16(u32 fontAddr, u8 page, u8 column);
void LCD_sendCmdToROM(u8 cmd);
void LCD_showPic12864(u8* dp);

#endif /* #ifndef __JLX_128_64_G_H__ */
