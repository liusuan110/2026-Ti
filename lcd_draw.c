/*
 * lcd_draw.c
 * LCD 图形绘制实现
 */

#include "lcd_draw.h"
#include <string.h>

/* 临时字符串缓冲 (用于数字转字符串, 省RAM) */
static char num_buf[12]; /* "4294967295\0" 最长11字符 */

/* ============ 工具函数 ============ */

/* 清除指定 page 范围 (page_start ~ page_end, 含) */
void LCD_clearPages(u8 page_start, u8 page_end)
{
    u8 p, c;
    for (p = page_start; p <= page_end; p++) {
        LCD_setAddr(p, 0);
        for (c = 0; c < 128; c++) {
            LCD_writeData(0x00);
        }
    }
}

/*
 * 在指定列画竖线段
 * y_top, y_bottom: 绝对像素坐标 (0~63)
 * 直接写对应 page 的数据
 */
void LCD_drawVLine(u8 column, u8 y_top, u8 y_bottom)
{
    u8 page_start, page_end;
    u8 p;
    u8 data, page_y_start, page_y_end, shift;

    if (y_top > y_bottom) return;
    if (y_bottom > 63) y_bottom = 63;

    page_start = y_top >> 3;     /* y_top / 8 */
    page_end   = y_bottom >> 3;  /* y_bottom / 8 */

    for (p = page_start; p <= page_end; p++) {
        data = 0xFF;
        page_y_start = p << 3;     /* p * 8 */
        page_y_end   = page_y_start + 7;

        /* 裁剪顶部 */
        if (page_y_start < y_top) {
            shift = y_top - page_y_start;
            data &= (0xFF << shift);
        }
        /* 裁剪底部 */
        if (page_y_end > y_bottom) {
            shift = page_y_end - y_bottom;
            data &= (0xFF >> shift);
        }

        LCD_setAddr(p, column);
        LCD_writeData(data);
    }
}

/*
 * 画单个像素点
 * x: 列 (0~127), y: 行 (0~63)
 * JLX12864G 无读回, 同一 page/column 的多次写入会互相覆盖
 * 适用于离散波形点绘制 (每列最多一个点)
 */
void LCD_drawDot(u8 x, u8 y)
{
    u8 page = y >> 3;
    u8 bit  = y & 0x07;
    LCD_setAddr(page, x);
    LCD_writeData(1 << bit);
}

/*
 * 画频谱柱状图
 * mag[]:   归一化到 0~47 的模值
 * count:   柱子数
 * bar_w:   每柱宽度
 * gap:     柱间间距
 * x_offset: 起始列
 */
void LCD_drawBars(const u8 mag[], u8 count, u8 bar_w, u8 gap, u8 x_offset)
{
    u8 p, x;
    u8 i = 0;
    u8 in_bar_col = 0;
    u8 group_pos = 0;
    u8 bar_height = 0;
    u8 group_w;

    if (bar_w == 0) return;
    group_w = bar_w + gap;

    /* 按页遍历，利用自动列递增机制，将 768 次坐标设置减少为 6 次 */
    for (p = 2; p <= 7; p++) {
        u8 page_y_start = (u8)(p << 3);
        
        LCD_setAddr(p, 0);

        for (x = 0; x < 128; x++) {
            u8 data = 0x00;
            u8 y_top = 64;

            if (x >= x_offset && count > 0) {
                uint16_t rel = (uint16_t)(x - x_offset);
                i = (u8)(rel / group_w);
                group_pos = (u8)(rel % group_w);

                if (i < count && group_pos < bar_w) {
                    in_bar_col = 1;
                    bar_height = mag[i];
                    if (bar_height > 47) bar_height = 47;
                    if (bar_height > 0) {
                        y_top = (u8)(16 + (47 - bar_height));
                    }
                } else {
                    in_bar_col = 0;
                }
            } else {
                in_bar_col = 0;
            }

            if (in_bar_col && bar_height > 0 && y_top <= (page_y_start + 7)) {
                u8 ys = (y_top > page_y_start) ? y_top : page_y_start;
                data = (u8)(0xFF << (ys - page_y_start));
            }

            LCD_writeData(data);
        }
    }
}

/* ============ 数字显示函数 ============ */

/* uint32 转字符串 (右对齐, 返回字符串指针) */
static char* uint_to_str(uint32_t val)
{
    int8_t i = 10;
    num_buf[11] = '\0';

    if (val == 0) {
        num_buf[10] = '0';
        return &num_buf[10];
    }

    while (val > 0 && i >= 0) {
        num_buf[i] = '0' + (val % 10);
        val /= 10;
        i--;
    }

    return &num_buf[i + 1];
}

/* 显示无符号整数 */
void LCD_showUint(u8 page, u8 column, uint32_t val, u8 decimal)
{
    char display_buf[16];
    char *p;
    int8_t len, dot_pos, di;

    p = uint_to_str(val);
    len = (int8_t)strlen(p);

    if (decimal == 0 || len <= decimal) {
        /* 无小数 或 整数部分为0 */
        if (decimal > 0 && len <= decimal) {
            /* 需要补零: 如 val=5, decimal=2 -> "0.05" */
            di = 0;
            display_buf[di++] = '0';
            display_buf[di++] = '.';
            /* 补前导零 */
            {
                int8_t zeros = decimal - len;
                while (zeros > 0) {
                    display_buf[di++] = '0';
                    zeros--;
                }
            }
            /* 拷贝数字 */
            while (*p) {
                display_buf[di++] = *p++;
            }
            display_buf[di] = '\0';
        } else {
            /* 直接显示 */
            strcpy(display_buf, p);
        }
    } else {
        /* 插入小数点 */
        dot_pos = len - decimal;
        di = 0;
        {
            int8_t si;
            for (si = 0; si < len; si++) {
                if (si == dot_pos) {
                    display_buf[di++] = '.';
                }
                display_buf[di++] = p[si];
            }
        }
        display_buf[di] = '\0';
    }

    LCD_showGB2312Str(page, column, (u8 *)display_buf);
}

/*
 * 显示带标签和单位的测量值
 * 例: LCD_showMeasure(2, 0, "Freq:", 20125, 0, "Hz")
 *     -> 屏幕显示 "Freq:20125Hz"
 */
void LCD_showMeasure(u8 page, u8 column, const char *label,
                     uint32_t val, u8 decimal, const char *unit)
{
    u8 col = column;

    /* 显示标签 */
    LCD_showGB2312Str(page, col, (u8 *)label);
    col += strlen(label) * 8; /* ASCII 字符宽 8 像素 */

    /* 显示数值 */
    LCD_showUint(page, col, val, decimal);

    /* 计算数值占用的宽度, 粗略估计 */
    {
        char *p = uint_to_str(val);
        u8 num_len = (u8)strlen(p);
        if (decimal > 0) num_len++; /* 小数点 */
        col += num_len * 8;
    }

    /* 显示单位 */
    LCD_showGB2312Str(page, col, (u8 *)unit);
}
