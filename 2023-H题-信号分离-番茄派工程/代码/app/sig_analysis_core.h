#ifndef SIG_ANALYSIS_CORE_H
#define SIG_ANALYSIS_CORE_H

#include <stdint.h>
#include "h_config.h"

/* 信号分析"算法核"(纯 C, 无 CMSIS/MATHACL 依赖) —— 在幅度谱上做:
 *   寻两个峰 -> 抛物线插值(亚 bin 频率) -> 谐波比判正弦/三角 (含 fB=3fA 特判).
 * 既供目标端(sig_analysis.c, 配 CMSIS-DSP FFT)调用, 也可在 PC 上用合成数据验证
 * (见 host_test/test_sig_analysis.c). 不填相位(phase_q31 由目标端 MATHACL 补). */

/* 谐波判别阈值: 基波三bin能量 > N次谐波三bin能量 * 阈值 => 正弦, 否则三角.
 * 三角波 3次谐波约 1/9 基波(比值~9), 5次约 1/25(比值~25); 阈值取其上、远低于正弦 => 40. */
#ifndef SIGCORE_HARM_THRESH
#define SIGCORE_HARM_THRESH  40u
#endif

void SigCore_analyze(const uint16_t *mag, uint16_t nhalf, uint32_t fs_hz, h_result_t *res);

/* ANA-2 块归一化: 给定加窗后时域峰值绝对值, 返回应左移的位数, 使峰值落入
 * (0.5, 1.0] 满量程区间(不溢出 q15), 拉高 FFT 谱线 SNR. maxabs=0 返回 0. 纯函数, 可单测. */
int32_t SigCore_normShift(uint32_t maxabs);

#endif /* SIG_ANALYSIS_CORE_H */
