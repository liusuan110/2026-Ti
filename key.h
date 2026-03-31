/*
 * key.h
 * 按键驱动 (P1.3, 低电平有效, 内部上拉)
 * 单键中断事件
 */

#ifndef __KEY_H__
#define __KEY_H__

#include "config.h"

/* 按键事件 */
#define KEY_NONE    0
#define KEY_1_SHORT 1   /* KEY1 短按 (P1.3) */
#define KEY_2_SHORT 2   /* 兼容保留, 当前未使用 */

/* 初始化按键引脚 + 端口中断 (内部上拉, 下降沿触发) */
void Key_init(void);

/*
 * 获取一次按键事件 (中断中置位, 主循环中读取)
 * 返回: KEY_NONE / KEY_1_SHORT / KEY_2_SHORT
 */
uint8_t Key_getEvent(void);

/*
 * 兼容接口: 读取一次按键事件
 * 返回: KEY_NONE / KEY_1_SHORT / KEY_2_SHORT
 */
uint8_t Key_scan(void);

#endif /* __KEY_H__ */
