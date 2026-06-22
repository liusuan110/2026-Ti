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

/* 频率吸附: 把测得频率四舍五入到最近 H_FREQ_STEP_HZ 网格, 并夹到 [MIN,MAX].
 * 依据: 题目所有输入都在 5kHz 整数倍上 -> 吸附中和片内 SYSOSC ±2% 的 fs 绝对误差,
 *       使 AD9959 输出精确网格频率、PLL 只需拉两晶振 ppm 级差. 纯函数, 可单测.
 * 注意: 仅在固件层(SigAnalysis_run, 受 H_FREQ_SNAP_GRID 控)调用, 不污染 SigCore_analyze
 *       的原始插值(离 bin 精度测试仍测原始值). */
uint32_t SigCore_snapFreq(uint32_t f_hz);

#endif /* SIG_ANALYSIS_CORE_H */
