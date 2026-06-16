#ifndef H_CONFIG_H
#define H_CONFIG_H

#include <stdint.h>

/* ===================== 2023 H题 信号分离 — 全局参数 =====================
 * 平台: 番茄派 MSPM0G3507 (Cortex-M0+ @80MHz, 双12位4MSPS ADC, MATHACL, 无FPU,
 *       128KB Flash / 32KB SRAM)
 * 方案(再生法): MSPM0 做 FFT 分析(频率/波形/相位) + AD9959 重建 A'/B' + 软件PLL锁相
 * 参数依据: 2026 北京电设培训3(周润华) —— fs=1.024MSa/s, N=1024, fRes=1kHz
 * ====================================================================== */

/* 系统时钟: 40MHz HFCLKEXT ×4 /2 = 80MHz */
#define H_SYS_CLK_HZ        80000000UL

/* 采样: 要分辨到 100kHz 三角波的 5 次谐波 -> fs>1MSa/s; N 取 2 的幂 */
#define H_ADC_FS_HZ         1024000UL
#define H_FFT_N             1024U
#define H_FREQ_RES_HZ       (H_ADC_FS_HZ / H_FFT_N)   /* = 1000 Hz */

/* 候选/分析频率范围: 10~100kHz, 5kHz 步进 */
#define H_FREQ_MIN_HZ       10000U
#define H_FREQ_MAX_HZ       100000U
#define H_FREQ_STEP_HZ      5000U
#define H_FREQ_CAND_N       19U      /* (100-10)/5 + 1 */

/* 两路被分离信号 */
#define H_NUM_SIGNALS       2U

/* 波形类型 */
typedef enum { H_WAVE_SINE = 0, H_WAVE_TRIANGLE = 1 } h_wave_t;

/* 单路信号分析结果 */
typedef struct {
    uint32_t freq_hz;     /* 频率 (Hz) */
    uint16_t amp;         /* 幅度 (FFT 幅值, 相对值) */
    int32_t  phase_q31;   /* 相位 (MATHACL 归一化角, 满量程对应 ±pi) */
    h_wave_t wave;        /* 波形类型 */
    uint8_t  valid;       /* 是否有效 */
} h_sig_t;

/* 整体分析结果: 两路, 按频率升序 [0]=低频A, [1]=高频B */
typedef struct {
    h_sig_t sig[H_NUM_SIGNALS];
} h_result_t;

#endif /* H_CONFIG_H */
