/*
 * lcd_draw.h
 * LCD 图形绘制扩展 (基于 JLX12864G 驱动)
 * - 画像素点
 * - Bresenham 画线
 * - 画矩形柱 (频谱用)
 * - 显示整数字符串
 *
 * JLX12864G 结构: 128 x 64 像素
 * 8 个 page (每 page = 8行), 128 列
 * 写入时纵向 1 字节 = 8 个像素 (LSB 在上)
 *
 * 注意: 此屏无读回功能, 需要用显存缓冲
 *       但 G2553 RAM 不够做全屏缓冲 (需1024B)
 *       折中方案: 只在画波形/频谱时使用局部缓冲
 */

#ifndef __LCD_DRAW_H__
#define __LCD_DRAW_H__

#include "JLX12864G.h"

/* ============ 局部显存 (用于波形/频谱绘制区域) ============ */

/*
 * 波形/频谱绘制区域: page 2~7 (y=16..63, 共48像素高)
 * 宽度: 128 列
 * 缓冲: 6 pages x 128 bytes = 768 bytes → 太大!
 *
 * 降级方案: 逐列绘制, 每列只缓冲 6 字节
 * 或者分区域: 只缓冲 128 列中有效部分
 *
 * 最终方案: 不用显存, 直接逐列计算并写入
 */

/* 清除指定 page 范围 */
void LCD_clearPages(u8 page_start, u8 page_end);

/* 在指定列画一个竖线段 (y_top ~ y_bottom, 0-based) */
void LCD_drawVLine(u8 column, u8 y_top, u8 y_bottom);

/*
 * 画频谱柱状图
 * mag[]:   模值数组 (已归一化到 0~47)
 * count:   柱子数
 * bar_w:   每个柱子宽度 (像素)
 * gap:     柱间间距 (像素)
 * x_offset: 起始列
 */
void LCD_drawBars(const u8 mag[], u8 count, u8 bar_w, u8 gap, u8 x_offset);

/*
 * 将无符号整数转换为字符串并显示
 * page, column: LCD 位置
 * val: 要显示的值
 * div10: 是否在末尾插入小数点 (用于显示如 "12.50")
 *        0 = 不插入, 1 = 最后1位为小数, 2 = 最后2位为小数
 */
void LCD_showUint(u8 page, u8 column, uint32_t val, u8 div10);

/*
 * 显示带单位的测量值
 * page, column: LCD 位置
 * label: 前缀标签 (如 "Freq:")
 * val: 数值
 * decimal: 小数位数 (0/1/2)
 * unit: 后缀单位 (如 "Hz")
 */
void LCD_showMeasure(u8 page, u8 column, const char *label,
                     uint32_t val, u8 decimal, const char *unit);

/* 画单个像素点 (x: 0~127, y: 0~63) */
void LCD_drawDot(u8 x, u8 y);

#endif /* __LCD_DRAW_H__ */
