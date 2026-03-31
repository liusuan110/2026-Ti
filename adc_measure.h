/*
 * adc_measure.h
 * =============================================
 * ADC10 采样模块 — MSP430G2553
 * =============================================
 *
 * 功能概述:
 *   提供三种 ADC 采样模式, 供不同页面调用:
 *
 *   1. ADC_measureVpp()             — 多窗口鲁棒 Vpp/Vrms
 *      用途: 连续采样并做 Top-K/Bottom-K + 窗口中值聚合
 *      优势: 抑制毛刺点, 高频/低频下稳定性更好
 *      调用者: PAGE_VPP (任务7)
 *
 *   2. ADC_sampleToBuffer()         — 连续采样到数组
 *      用途: 均匀采集 N 个点存入 buffer, 用于 FFT
 *      调用者: PAGE_FFT (任务10)
 *
 *   3. ADC_sampleToBufferAdaptive() — 自适应间隔采样到数组
 *      用途: 根据信号频率计算采样间隔, 覆盖完整周期
 *      调用者: PAGE_WAVE (任务9) — 过采样后由软件零交叉触发截取
 *
 * 硬件配置:
 *   - 内部 2.5V 基准电压 (REF2_5V)
 *   - 10 位分辨率 (0~1023)
 *   - ADC10OSC 时钟源 (~5MHz)
 *   - 通道由调用者指定 (INCH_0 / INCH_7 等)
 */

#ifndef __ADC_MEASURE_H__
#define __ADC_MEASURE_H__

#include "config.h"

/* ============ 数据结构 ============ */

/* Vpp/Vrms 测量结果 (单位: mV) */
typedef struct {
    uint16_t vpp_mv;    /* 峰峰值 mV: (ADC_max - ADC_min) * 2500 / 1023 */
    uint16_t vrms_mv;   /* 有效值 mV: Vpp / (2√2) ≈ Vpp * 1000 / 2828 */
} VppResult;

/* ============ 函数声明 ============ */

/* 初始化 ADC 引脚为模拟输入 (P1.0, P1.7) */
void ADC_init(void);

/*
 * 流式采样测 Vpp/Vrms (不需要数组, 省 RAM)
 * channel: ADC 通道 (如 ADC_CH_UO2)
 * count:   总采样点预算 (函数内部会拆分为多个窗口并做中值聚合)
 * result:  结果存放结构体地址
 */
void ADC_measureVpp(uint16_t channel, uint16_t count, VppResult *result);

/*
 * 连续采样 N 个点到 buffer, 无额外延时 (最快速度)
 * channel: ADC 通道
 * buf:     目标 int16_t 数组
 * len:     采样点数
 */
void ADC_sampleToBuffer(uint16_t channel, int16_t *buf, uint16_t len);

/*
 * 自适应间隔连续采样:
 * 根据 freq_hz 自动计算每两个采样之间的 CPU 延时,
 * 使 len 个点的总窗口覆盖 >= 1 个信号周期.
 *
 * channel:  ADC 通道
 * buf:      目标 int16_t 数组
 * len:      采样点数 (波形显示时传 WAVE_RAW_POINTS=96)
 * freq_hz:  信号频率 (Hz), 若传 0 则不插入延时
 *
 * 注意: 本函数只负责采样, 不做触发同步.
 *       波形稳定由调用方的软件零交叉触发实现 (见 display.c).
 */
void ADC_sampleToBufferAdaptive(uint16_t channel, int16_t *buf, uint16_t len, uint32_t freq_hz);

#endif /* __ADC_MEASURE_H__ */
