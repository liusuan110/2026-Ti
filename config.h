/*
 * config.h
 * =============================================
 * 2026 电子信息杯 - 多信号发生器 单片机部分
 * 全局引脚分配 & 系统配置
 * =============================================
 * 引脚分配总表 (MSP430G2553 DIP-20):
 *
 *  P1.0 (A0)      -> ADC: U_o2 正弦波 / U_o4 余弦波 (按页面复用)
 *  P1.1 (TA0.0)   -> 捕获: U_o1 方波 (频率测量 + 波形同步触发)
 *  P1.2 (TA0.1)   -> 捕获: U_o3 窄脉冲 (占空比测量)
 *  P1.3            -> 按键 KEY1 (上拉输入, 单键)
 *  P1.4            -> LCD SCLK    (已占用)
 *  P1.5            -> LCD SDA     (已占用)
 *  P1.6            -> LCD ROM_CS  (已占用)
 *  P1.7 (A7)      -> ADC: U_o3 窄脉冲 (FFT采样, 避免与捕获模式冲突)
 *  P2.0            -> LCD RS      (已占用)
 *  P2.1            -> LCD RST     (已占用)
 *  P2.2            -> LCD CS      (已占用)
 *  P2.3            -> LCD ROM_IN  (已占用)
 *  P2.4            -> LCD ROM_OUT (已占用)
 *  P2.5            -> LCD ROM_SCK (已占用)
 *  说明: U_o2 与 U_o4 共享 P1.0(A0), 由状态机按当前页面选择测量功能
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <msp430g2553.h>

/* ============ 系统时钟频率 ============ */
#define SYS_FREQ        16000000UL  /* 16MHz DCO */

/* ============ ADC 通道定义 ============ */
#define ADC_CH_UO2      INCH_0      /* P1.0 / A0:  U_o2 正弦波 */
#define ADC_CH_UO4      INCH_0      /* P1.0 / A0:  U_o4 余弦波(复用) */
#define ADC_CH_UO3_FFT  INCH_7      /* P1.7 / A7:  U_o3 FFT采样 */

#define ADC_PIN_UO2     BIT0        /* P1.0 */
#define ADC_PIN_UO4     BIT0        /* P1.0 复用 */
#define ADC_PIN_UO3_FFT BIT7        /* P1.7 */

/* ============ Timer 捕获引脚 ============ */
#define CAP_FREQ_PIN    BIT1        /* P1.1 / TA0.0: 方波频率捕获 */
#define CAP_DUTY_PIN    BIT2        /* P1.2 / TA0.1: 窄脉冲占空比 */

/* ============ 按键引脚 (P1 端口, 单键) ============ */
#define KEY1_BIT        BIT3        /* P1.3 */
#define KEY2_BIT        0x00        /* 未使用, 仅为兼容保留 */

/* ============ 页面枚举 ============ */
typedef enum {
    PAGE_INFO = 0,  /* 任务5: 显示队号姓名 */
    PAGE_FREQ,      /* 任务7: Uo1/Uo2 频率/幅值切换显示 */
    PAGE_VPP,       /* 任务9: Uo4 Vpp/Vrms 切换显示 */
    PAGE_DUTY,      /* 预留调试页(不参与任务三显示顺序) */
    PAGE_WAVE,      /* 任务8: Uo3/Uo5 波形显示 */
    PAGE_FFT,       /* 任务10: Uo1 频谱显示 */
    PAGE_COUNT
} PageID;

/* ============ 频率中值滤波 ============ */
#define FREQ_MED_SIZE   5           /* 频率测量中值滤波窗口 (奇数) */

/* ============ 波形采样 ============ */
#define WAVE_RAW_POINTS    96       /* 波形页 ADC 采样点数 (最高速, 覆盖 ~5-6 周期 @18kHz) */
#define WAVE_DISPLAY_CYCLES 2       /* 自适应采样: 屏幕目标显示周期数 */

/* ============ FFT 参数 ============ */
#define FFT_N           32          /* 32点FFT (节省RAM) */
#define FFT_LOG2N       5           /* log2(32) = 5 */

/* ============ ADC 参考电压 (mV) ============ */
#define ADC_VREF_MV     3300        /* 使用 AVCC (3.3V) 作为基准, 修复3V信号削顶问题 */

/* ============ 定点数辅助宏 ============ */
/* 将 ADC 10位值转换为 mV: val * 3300 / 1023 */
/* 避免溢出: 先乘后除, 用 int32_t */
#define ADC_TO_MV(val)  ((int32_t)(val) * ADC_VREF_MV / 1023)

#endif /* __CONFIG_H__ */
