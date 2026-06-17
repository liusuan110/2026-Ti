#ifndef RECONSTRUCT_H
#define RECONSTRUCT_H

#include <stdint.h>
#include "h_config.h"
#include "reconstruct_core.h"

/* 用 AD9959 重建 A'/B' (再生法). idx: 0->A'(CH0), 1->B'(CH1).
 * 三角波: AD9959 只出正弦, 三角靠硬件(比较器整型->积分器), 见 上板联调清单.md. */

void Reconstruct_init(void);
void Reconstruct_set(uint8_t idx, const h_sig_t *sig);     /* 按分析结果建一路 */
void Reconstruct_setPhase14(uint8_t idx, uint16_t phase14); /* PLL: 直接写14位相位字 */
void Reconstruct_setFreqHz(uint8_t idx, float freq_hz);
/* PLL 锁相用: 把 idx 路的 32 位频率控制字直写 DDS (整数, 不走 float, 保亚-Hz 精度). */
void Reconstruct_setFtw(uint8_t idx, uint32_t ftw);
/* 发挥②: 把 idx 路相位设为 (ref_cpow + deg) 对应的相位字 —— B' 相对 A' 的相位差(0..180°) */
void Reconstruct_setPhaseDiffDeg(uint8_t idx, uint16_t ref_cpow, int32_t deg);

#endif /* RECONSTRUCT_H */
