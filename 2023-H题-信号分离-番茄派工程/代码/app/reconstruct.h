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

#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
/* 模拟鉴相模式: AD9959 4 通道. CH0/CH1=输出 A'/B'(相位0); CH2/CH3=正交本振(给外部乘法器鉴相). */
void Reconstruct_initAnalog(void);
/* 设 idx 路: 输出通道(CH0/1)与正交本振通道(CH2/3)同设为 freq_hz; 本振相位=ref_phase14(90°+发挥②偏移).
 * 不立即生效, 由 Reconstruct_update 批量 IOUpdate(4 通道原子更新). */
void Reconstruct_setAnalog(uint8_t idx, float freq_hz, uint16_t ref_phase14);
/* 批量生效(AD9959 IOUpdate) */
void Reconstruct_update(void);
#endif

#endif /* RECONSTRUCT_H */
