#include "sig_analysis.h"
#include "sig_analysis_core.h"
#include "mathacl_util.h"
#include "arm_math.h"
#include "arm_const_structs.h"

/* 大缓冲放静态区(不放栈, 避开 SHR 工程的爆栈坑) */
static q15_t    s_fft[2u * H_FFT_N];        /* 交错复数, in-place FFT (4KB) */
static uint16_t s_mag[H_FFT_N / 2u];        /* 半谱幅度 (1KB) */

void SigAnalysis_init(void)
{
    /* 预留: 窗函数表等(当前用矩形窗 + 抛物线插值) */
}

uint8_t SigAnalysis_run(const uint16_t *buf, uint16_t n, h_result_t *res)
{
    uint16_t i, s;
    uint16_t nn    = (n < (uint16_t)H_FFT_N) ? n : (uint16_t)H_FFT_N;
    const uint16_t nhalf = (uint16_t)(H_FFT_N / 2u);

    /* 1) 转 q15 交错复数, 去直流(减 2048), 左移放大到 q15 量级 */
    for (i = 0u; i < nn; i++) {
        s_fft[2u * i]      = (q15_t)(((int32_t)buf[i] - 2048) << 4);
        s_fft[2u * i + 1u] = 0;
    }
    for (; i < (uint16_t)H_FFT_N; i++) {
        s_fft[2u * i]      = 0;
        s_fft[2u * i + 1u] = 0;
    }

    /* 2) 定点 FFT(CMSIS-DSP)+ 半谱求模 */
    arm_cfft_q15(&arm_cfft_sR_q15_len1024, s_fft, 0, 1);
    arm_cmplx_mag_q15(s_fft, (q15_t *)s_mag, nhalf);

    /* 3) 算法核: 两路 频率/幅度/波形 */
    SigCore_analyze(s_mag, nhalf, (uint32_t)H_ADC_FS_HZ, res);

    /* 4) 相位: 峰值 bin 复数 atan2(MATHACL). 由频率反算整数 bin */
    for (s = 0u; s < (uint16_t)H_NUM_SIGNALS; s++) {
        uint32_t kf = (uint32_t)res->sig[s].freq_hz * H_FFT_N / (uint32_t)H_ADC_FS_HZ;
        if (kf < nhalf) {
            res->sig[s].phase_q31 = MathUtil_atan2_q31(
                ((int32_t)s_fft[2u * kf])      << 16,
                ((int32_t)s_fft[2u * kf + 1u]) << 16);
        }
    }
    return (uint8_t)H_NUM_SIGNALS;
}
