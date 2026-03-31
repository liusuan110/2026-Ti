#include "JLX12864G.h"

static void pinInit();
static u8 getDataFromROM();

// 引脚初始化
static void pinInit() {
    GPIO_pinMode(LCD_ROM_IN_PIN, OUTPUT);
    GPIO_pinMode(LCD_ROM_OUT_PIN, INPUT);
    GPIO_pinMode(LCD_ROM_SCK_PIN, OUTPUT);
    GPIO_pinMode(LCD_ROM_CS_PIN, OUTPUT);
    GPIO_pinMode(LCD_SCLK_PIN, OUTPUT);
    GPIO_pinMode(LCD_SDA_PIN, OUTPUT);
    GPIO_pinMode(LCD_RS_PIN, OUTPUT);
    GPIO_pinMode(LCD_RST_PIN, OUTPUT);
    GPIO_pinMode(LCD_CS_PIN, OUTPUT);
    LCD_ROM_IN_CLR();
    LCD_ROM_SCK_CLR();
    LCD_ROM_CS_SET();
    LCD_SCLK_CLR();
    LCD_SDA_CLR();
    LCD_RS_CLR();
    LCD_RST_SET();
    LCD_CS_SET();
}

// 初始化LCD
void LCD_init() {
    pinInit();
    LCD_RST_CLR(); // 低电平复位
    delay(10);
    LCD_RST_SET();
    delay(20); // 等待复位完毕
    LCD_writeCmd(0xE2); // 软复位
    delay(5);
    LCD_writeCmd(0x2C); // 升压步骤1
    delay(5);
    LCD_writeCmd(0x2E); // 升压步骤2
    delay(5);
    LCD_writeCmd(0x2F); // 升压步骤3
    delay(5);
    LCD_writeCmd(0x23); // 粗调对比度，可设置范围 0x20～0x27
    LCD_writeCmd(0x81); // 微调对比度
    LCD_writeCmd(0x28); // 微调对比度的值，可设置范围 0x00～0x3F
    LCD_writeCmd(0xA2); // 1/9 偏压比(bias)
    LCD_writeCmd(0xC8); // 行扫描顺序：从上到下
    LCD_writeCmd(0xA0); // 列扫描顺序：从左到右
    LCD_writeCmd(0x40); // 起始行：第一行开始
    LCD_writeCmd(0xAF); // 开显示
}

// 向LCD逐字节写入命令
void LCD_writeCmd(u8 cmd) {
    u8 i;
    LCD_CS_CLR();
    LCD_RS_CLR();
    LCD_DELAY(1); // 加少量延时
    for (i = 0; i < 8; i++) {
        LCD_SCLK_CLR();
        if (cmd & 0x80) {
            LCD_SDA_SET();
        }
        else {
            LCD_SDA_CLR();
        }
        LCD_DELAY(1); // 加少量延时
        LCD_SCLK_SET();
        LCD_DELAY(1); // 加少量延时
        cmd <<= 1;
    }
    LCD_DELAY(1); // 加少量延时
    LCD_CS_SET();
    LCD_DELAY(1); // 加少量延时
}

// 向LCD逐字节写入数据
void LCD_writeData(u8 data) {
    u8 i;
    LCD_CS_CLR();
    LCD_RS_SET();
    LCD_DELAY(1); // 加少量延时
    for (i = 0; i < 8; i++) {
        LCD_SCLK_CLR();
        if (data & 0x80) {
            LCD_SDA_SET();
        }
        else {
            LCD_SDA_CLR();
        }
        LCD_DELAY(1); // 加少量延时
        LCD_SCLK_SET();
        LCD_DELAY(1); // 加少量延时
        data <<= 1;
    }
    LCD_DELAY(1); // 加少量延时
    LCD_CS_SET();
    LCD_DELAY(1); // 加少量延时
}

// 清屏
void LCD_clear() {
    u8 i, j;
    for (i = 0; i < 9; i++) {
        LCD_writeCmd(0xB0 + i);
        LCD_writeCmd(0x10);
        LCD_writeCmd(0x00);
        for (j = 0; j < 132; j++) {
            LCD_writeData(0x00);
        }
    }
}

// 定位光标位置
void LCD_setAddr(u8 page, u8 column) {
    LCD_writeCmd(0xB0 + page); // 设置页地址，每 8 行为一页，全屏共 64 行，被分成 8 页
    LCD_writeCmd(0x10 + (column >> 4 & 0x0F)); // 设置列地址的高 4 位
    LCD_writeCmd(column & 0x0F); // 设置列地址的低 4 位
}

// 显示GB2312字符
// 16x16
void LCD_showGB2312Str(u8 page, u8 column, u8* text) {
    u32 fontAddr;
    u8 i = 0;
    while ((text[i] > 0x00)) {
        if (((text[i] >= 0xB0) && (text[i] <= 0xF7)) && (text[i + 1] >= 0xA1)) {
            fontAddr = (text[i] - 0xB0) * 94;
            fontAddr += (text[i + 1] - 0xA1) + 846;
            fontAddr = (u32)(fontAddr * 32);
            LCD_getAndWrite16x16(fontAddr, page, column); // 从指定地址读出数据写到液晶屏指定(page,column)坐标中
            i += 2;
            column += 16;
        }
        else if (((text[i] >= 0xA1) && (text[i] <= 0xA3)) && (text[i + 1] >= 0xA1)) {
            fontAddr = (text[i] - 0xA1) * 94;
            fontAddr += (text[i + 1] - 0xA1);
            fontAddr = (u32)(fontAddr * 32);
            LCD_getAndWrite16x16(fontAddr, page, column); // 从指定地址读出数据写到液晶屏指定(page,column)坐标中
            i += 2;
            column += 16;
        }
        else if ((text[i] >= 0x20) && (text[i] <= 0x7E)) {
            fontAddr = (text[i] - 0x20);
            fontAddr = (u32)(fontAddr * 16);
            fontAddr = (u32)(fontAddr + 0x3CF80);
            LCD_getAndWrite8x16(fontAddr, page, column); // 从指定地址读出数据写到液晶屏指定(page,column)坐标中
            i += 1;
            column += 8;
        }
        else {
            i++;
        }
    }
}

// 从字库中获取数据并写入LCD
void LCD_getAndWrite16x16(u32 fontAddr, u8 page, u8 column) {
    u8 i, j, dispData;
    LCD_ROM_CS_CLR();
    LCD_DELAY(1); // 加少量延时
    LCD_sendCmdToROM(0x03);
    LCD_sendCmdToROM((fontAddr & 0xff0000) >> 16); // 地址的高 8 位,共 24 位
    LCD_sendCmdToROM((fontAddr & 0xff00) >> 8); // 地址的中 8 位,共 24 位
    LCD_sendCmdToROM(fontAddr & 0xff); // 地址的低 8 位,共 24 位
    for (j = 0; j < 2; j++) {
        LCD_setAddr(page + j, column);
        for (i = 0; i < 16; i++) {
            dispData = getDataFromROM();
            LCD_writeData(dispData); // 写数据到 LCD,每写完 1 字节的数据后列地址自动加 1
        }
    }
    LCD_DELAY(1); // 加少量延时
    LCD_ROM_CS_SET();
    LCD_DELAY(1); // 加少量延时
}

// 从字库中获取数据并写入LCD
void LCD_getAndWrite8x16(u32 fontAddr, u8 page, u8 column) {
    u8 i, j, dispData;
    LCD_ROM_CS_CLR();
    LCD_DELAY(1); // 加少量延时
    LCD_sendCmdToROM(0x03);
    LCD_sendCmdToROM((fontAddr & 0xff0000) >> 16); // 地址的高 8 位,共 24 位
    LCD_sendCmdToROM((fontAddr & 0xff00) >> 8); // 地址的中 8 位,共 24 位
    LCD_sendCmdToROM(fontAddr & 0xff); // 地址的低 8 位,共 24 位
    for (j = 0; j < 2; j++) {
        LCD_setAddr(page + j, column);
        for (i = 0; i < 8; i++) {
            dispData = getDataFromROM();
            LCD_writeData(dispData); //写数据到 LCD,每写完 1 字节的数据后列地址自动加 1
        }
    }
    LCD_DELAY(1); // 加少量延时
    LCD_ROM_CS_SET();
    LCD_DELAY(1); // 加少量延时
}

// 从字库中获取数据
static u8 getDataFromROM() {
    u8 data = 0x00;
    u8 i;
    for (i = 0; i < 8; i++) {
        data <<= 1;
        LCD_ROM_SCK_CLR();
        LCD_DELAY(1);
        if (LCD_ROM_OUT_VAL()) {
            data |= 0x01;
        }
        LCD_ROM_SCK_SET();
        LCD_DELAY(1);
    }
    return data;
}

// 向字库发送命令
void LCD_sendCmdToROM(u8 cmd) {
    u8 i;
    for (i = 0; i < 8; i++) {
        LCD_ROM_SCK_CLR();
        if (cmd & 0x80) {
            LCD_ROM_IN_SET();
        }
        else {
            LCD_ROM_IN_CLR();
        }
        cmd <<= 1;
        LCD_DELAY(1);
        LCD_ROM_SCK_SET();
        LCD_DELAY(1);
    }
}

// 显示128x64点阵图像
void LCD_showPic12864(u8* dp) {
    u8 i, j;
    for (j = 0; j < 8; j++) {
        LCD_setAddr(j, 0);
        for (i = 0; i < 128; i++) {
            LCD_writeData(*dp); // 写数据到LCD,每写完一个8位的数据后列地址自动加1
            dp++;
        }
    }
}
