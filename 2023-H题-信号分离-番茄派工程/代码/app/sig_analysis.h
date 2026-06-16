#ifndef SIG_ANALYSIS_H
#define SIG_ANALYSIS_H

#include <stdint.h>
#include "h_config.h"

/* 信号分析: 对采集缓冲做定点 FFT(CMSIS-DSP), 寻两个峰 -> 频率/幅度/相位/波形.
 * 波形判别: 3 次谐波能量比 (正弦谐波小, 三角谐波大).
 * 相位: 峰值 bin 复数做 atan2 (MATHACL). */

void    SigAnalysis_init(void);
uint8_t SigAnalysis_run(const uint16_t *buf, uint16_t n, h_result_t *res);  /* 返回有效路数 */

#endif /* SIG_ANALYSIS_H */
