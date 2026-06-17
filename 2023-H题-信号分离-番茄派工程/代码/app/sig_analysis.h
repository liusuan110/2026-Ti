#ifndef SIG_ANALYSIS_H
#define SIG_ANALYSIS_H

#include <stdint.h>
#include "h_config.h"

/* 信号分析: 对采集缓冲做定点 FFT(CMSIS-DSP), 寻两个峰 -> 频率/幅度/相位/波形.
 * 波形判别: 3 次谐波能量比 (正弦谐波小, 三角谐波大).
 * 相位: 峰值 bin 复数做 atan2 (MATHACL). */

void    SigAnalysis_init(void);
uint8_t SigAnalysis_run(const uint16_t *buf, uint16_t n, h_result_t *res);  /* 返回有效路数 */

/* PLL 反馈相位提取: 对反馈缓冲(A'+B')做窗+FFT, 在给定 bin(取自对C的分析 res.sig[i].bin)
 * 处 atan2 得各路相位(Q31 归一化角)写入 out_q31. 须在 SigAnalysis_run 之后调用(会覆写内部FFT缓冲). */
void    SigAnalysis_phasesAtBins(const uint16_t *buf, uint16_t n,
                                 const uint16_t *bins, uint8_t nbins, int32_t *out_q31);

#endif /* SIG_ANALYSIS_H */
