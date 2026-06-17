#ifndef SIG_ACQ_H
#define SIG_ACQ_H

#include <stdint.h>
#include "h_config.h"

/* ADC12 + FIFO + DMA 块采集 (番茄派 ADC0, 12 位).
 * 软件触发 + repeat, fs ≈ 1.0~1.03 MSa/s (用 SYSOSC 32MHz + 采样窗 17 周期).
 * ⚠️ 实际 fs 需上板标定后写回 h_config.h 的 H_ADC_FS_HZ.
 * 实现/坑详见 sig_acq.c 顶部注释; 精确锁率(TIMG事件触发)为后续增强. */

void     SigAcq_init(void);
/* 第2路反馈 ADC(ADC1)初始化, 采重建 A'+B' 供 PLL 鉴相. ★引脚/同步见 sig_acq.c 与 上板联调清单. */
void     SigAcq_initFeedback(void);
/* 采 n 点 12 位样本(0..4095)到 buf, 返回实际点数.
 * 注意: buf 必须 4 字节对齐(DMA 按 32 位字写入); n 取偶数(FIFO 每字 2 样本). */
uint16_t SigAcq_capture(uint16_t *buf, uint16_t n);
/* 双路近同步采集: C->c_buf(ADC0), 反馈 A'+B'->fb_buf(ADC1). 两 buf 均须 4 字节对齐. */
uint16_t SigAcq_captureDual(uint16_t *c_buf, uint16_t *fb_buf, uint16_t n);

#endif /* SIG_ACQ_H */
