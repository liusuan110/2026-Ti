/*
 * timer_capture.h
 * Timer_A0 捕获模块
 * - CCR0 (P1.1/TA0.0): 方波频率捕获
 * - CCR1 (P1.2/TA0.1): 窄脉冲占空比双沿捕获
 */

#ifndef __TIMER_CAPTURE_H__
#define __TIMER_CAPTURE_H__

#include "config.h"

/* ============ 测量结果 (全局可读) ============ */
extern volatile uint32_t g_freq_hz;       /* 频率 Hz */
extern volatile uint8_t  g_freq_ready;    /* 频率数据就绪标志 */
extern volatile uint16_t g_freq_period;   /* 最新捕获周期 (timer ticks, 16MHz) */

extern volatile uint16_t g_duty_percent;  /* 占空比 x100 (如 1250 表示 12.50%) */
extern volatile uint8_t  g_duty_ready;    /* 占空比数据就绪标志 */

/* ============ 函数声明 ============ */

/* 初始化 Timer_A0 为连续模式, 开启 CCR0/CCR1 捕获 */
void Capture_init(void);

/* 启动频率捕获 (CCR0 上升沿) */
void Capture_startFreq(void);

/* 停止频率捕获 */
void Capture_stopFreq(void);

/* 启动占空比捕获 (CCR1 双沿) */
void Capture_startDuty(void);

/* 停止占空比捕获 */
void Capture_stopDuty(void);

/* 停止全部捕获, Timer 停止 */
void Capture_stopAll(void);

#endif /* __TIMER_CAPTURE_H__ */
