#ifndef RECONSTRUCT_H
#define RECONSTRUCT_H

#include <stdint.h>
#include "h_config.h"

/* 用 AD9959 重建 A'/B' (再生法). idx: 0->A'(CH0), 1->B'(CH1).
 * 三角波: AD9959 只出正弦, 三角靠硬件(比较器整型->积分器), 见 硬件/README. */

void Reconstruct_init(void);
void Reconstruct_set(uint8_t idx, const h_sig_t *sig);    /* 按分析结果建一路 */
void Reconstruct_setPhase14(uint8_t idx, uint16_t phase14); /* PLL/发挥②: 写14位相位字 */
void Reconstruct_setFreqHz(uint8_t idx, float freq_hz);

#endif /* RECONSTRUCT_H */
