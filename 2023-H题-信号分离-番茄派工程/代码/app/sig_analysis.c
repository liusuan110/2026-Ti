#include "sig_analysis.h"
#include "sig_analysis_core.h"
#include "mathacl_util.h"
#include "hann1024.h"
#include "arm_math.h"
#include "arm_const_structs.h"

/* 大缓冲放静态区(不放栈, 避开 SHR 工程的爆栈坑) */
static q15_t    s_fft[2u * H_FFT_N];        /* 交错复数, in-place FFT (4KB) */
static uint16_t s_mag[H_FFT_N / 2u];        /* 半谱幅度 (1KB) */

void SigAnalysis_init(void)
{
    /* 窗函数表 hann1024 为编译期常量, 无需初始化 */
}

uint8_t SigAnalysis_run(const uint16_t *buf, uint16_t n, h_result_t *res)
{
    uint16_t i, s;
    uint16_t nn    = (n < (uint16_t)H_FFT_N) ? n : (uint16_t)H_FFT_N;
    const uint16_t nhalf = (uint16_t)(H_FFT_N / 2u);

    /* 1) 去直流(减 2048) -> 乘汉宁窗(Q15) -> 暂存实部, 同时求峰值绝对值 */
    {
        uint16_t maxabs = 0u;
        int32_t  sh;
        for (i = 0u; i < nn; i++) {
            int32_t centered = (int32_t)buf[i] - 2048;
            int32_t windowed = (centered * (int32_t)hann1024[i]) >> 15;  /* ×Hann, 范围±2048 */
            int32_t a = (windowed < 0) ? -windowed : windowed;
            if ((uint16_t)a > maxabs) { maxabs = (uint16_t)a; }
            s_fft[2u * i]      = (q15_t)windowed;
            s_fft[2u * i + 1u] = 0;
        }
        for (; i < (uint16_t)H_FFT_N; i++) {
            s_fft[2u * i]      = 0;
            s_fft[2u * i + 1u] = 0;
        }
        /* ANA-2: 块归一化 —— 1Vpp 在 12bit/3.3V 上仅约 ±620 码, 固定 <<4 后谱线仅满量程
         * 3~8%, 量化噪声相对抬高致波形误判. 据实测峰值自适应左移, 把峰拉到接近满量程,
         * __SSAT 兜底防溢出, 显著提升谱线 SNR. (与旧版固定 <<4 的根本区别) */
        sh = SigCore_normShift((uint32_t)maxabs);
        if (sh > 0) {
            for (i = 0u; i < nn; i++) {
                int32_t v = (int32_t)s_fft[2u * i] << sh;
                s_fft[2u * i] = (q15_t)__SSAT(v, 16);
            }
        }
    }

    /* 2) 定点 FFT(CMSIS-DSP)+ 半谱求模 */
    arm_cfft_q15(&arm_cfft_sR_q15_len1024, s_fft, 0, 1);
    arm_cmplx_mag_q15(s_fft, (q15_t *)s_mag, nhalf);

    /* 3) 算法核: 两路 频率/幅度/波形(抛物线插值在汉宁窗下精度更高) */
    SigCore_analyze(s_mag, nhalf, (uint32_t)H_ADC_FS_HZ, res);

    /* 4) 相位: 直接用算法核给出的峰值 bin 取复数 atan2 (ANA-1: 不再由频率反算,
     *    避免反算整数 bin 与实际峰 bin 在取整边界差 1 而取到噪声 bin). */
    for (s = 0u; s < (uint16_t)H_NUM_SIGNALS; s++) {
        uint16_t kb = res->sig[s].bin;
        if (kb < nhalf) {
            res->sig[s].phase_q31 = MathUtil_atan2_q31(
                ((int32_t)s_fft[2u * kb])      << 16,   /* x = 实部 */
                ((int32_t)s_fft[2u * kb + 1u]) << 16);  /* y = 虚部 */
        }
    }
    return (uint8_t)H_NUM_SIGNALS;
}
