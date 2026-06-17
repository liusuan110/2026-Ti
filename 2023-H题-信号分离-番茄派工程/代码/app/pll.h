#ifndef PLL_H
#define PLL_H

#include <stdint.h>
#include "pll_core.h"

/* ============================================================================
 *  再生法软件锁相环 — 固件接口 (目标端胶水层, 包裹 pll_core 并施加到 AD9959)
 *
 *  闭环结构 (每路独立):
 *    原信号相位 phi_ref  ── 来自对混合信号 C 的 FFT/atan2
 *    重建相位   phi_fb   ── 来自把 A'/B' 引回【第 2 路 ADC】后同样 FFT/atan2
 *                          (★上板硬件项: A'+B' -> ADC 通道2, 见 上板联调清单.md)
 *    误差 -> II 型 PI -> FTW(频率) 微调 -> A' 频率/相位锁定到 A.
 *
 *  默认增益 (PLL_*_SHIFT) 由离板仿真 host_test/test_pll.c 整定;
 *  整定假设环路周期 dt≈5ms (即建议按 SYS-4 去掉 50ms 阻塞延时).
 *  若改变控制周期, 必须重跑 test_pll.c 重新整定这三个常数.
 * ========================================================================== */

#define PLL_KP_SHIFT     22       /* 比例增益移位 (整定值) */
#define PLL_KI_SHIFT     27       /* 积分增益移位 (整定值) */
#define PLL_INTEG_LIMIT  (1 << 18) /* 频差补偿限幅(FTW LSB), 远超 ppm 级所需 */

#define PLL_DDS_REF_HZ   500000000u /* AD9959 参考时钟 25MHz×20 */

void Pll_init(void);

/* 建立/重设第 idx 路的预调谐 FTW (由粗测频率算得).
 * CPOW 初值由 reconstruct 在 build 时写入(Reconstruct_set). */
void Pll_retune(uint8_t idx, uint32_t freq_hz);

/* 闭环一步:
 *   phi_ref_q31    : 原信号分量相位 (Q31 归一化角, ±180°=±2^31)
 *   phi_fb_q31     : 重建信号实测相位 (同上; 来自第2路ADC)
 *   phi_target_q31 : 目标相位差 (发挥②, 0=同相)
 * 内部跑 pll_core 并把新 FTW 施加到 DDS. 返回该路是否已锁定. */
uint8_t Pll_track(uint8_t idx, int32_t phi_ref_q31, int32_t phi_fb_q31,
                  int32_t phi_target_q31);

uint8_t Pll_locked(uint8_t idx);

#endif /* PLL_H */
