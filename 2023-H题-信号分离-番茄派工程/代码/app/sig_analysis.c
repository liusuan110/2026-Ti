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

/* 去直流(−2048) → 乘汉宁窗(Q15) → 块归一化(ANA-2) → 定点 FFT, 结果(交错复数)留 s_fft.
 * 供 对C的分析(SigAnalysis_run)与 对反馈的相位提取(SigAnalysis_phasesAtBins)共用. */
static void prep_fft(const uint16_t *buf, uint16_t n)
{
    uint16_t i;
    uint16_t nn     = (n < (uint16_t)H_FFT_N) ? n : (uint16_t)H_FFT_N;
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
    /* ANA-2: 据实测峰值自适应左移到接近满量程(__SSAT 兜底), 提升谱线 SNR */
    sh = SigCore_normShift((uint32_t)maxabs);
    if (sh > 0) {
        for (i = 0u; i < nn; i++) {
            int32_t v = (int32_t)s_fft[2u * i] << sh;
            s_fft[2u * i] = (q15_t)__SSAT(v, 16);
        }
    }
    arm_cfft_q15(&arm_cfft_sR_q15_len1024, s_fft, 0, 1);
}

uint8_t SigAnalysis_run(const uint16_t *buf, uint16_t n, h_result_t *res)
{
    uint16_t s;
    const uint16_t nhalf = (uint16_t)(H_FFT_N / 2u);

    prep_fft(buf, n);
    arm_cmplx_mag_q15(s_fft, (q15_t *)s_mag, nhalf);

    /* 算法核: 两路 频率/幅度/波形/峰值bin(抛物线插值在汉宁窗下精度更高) */
    SigCore_analyze(s_mag, nhalf, (uint32_t)H_ADC_FS_HZ, res);

    /* 相位: 直接用算法核给出的峰值 bin 取复数 atan2 (ANA-1: 不由频率反算) */
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

/* 反馈相位提取(PLL 锁相用): 对反馈缓冲(A'+B')做同样的 窗+归一化+FFT,
 * 在给定 bin 处 atan2 得各路相位(Q31 归一化角). bins 取自对 C 的分析(res.sig[i].bin),
 * 故无需再找峰. ★会覆写内部 FFT 缓冲, 必须在 SigAnalysis_run(对C)之后调用.
 * 关键: C 与反馈两次都用同一套 窗+FFT+同一 bin, 窗/bin 偏移等公共项在 (ref−fb) 差分中抵消,
 *       故相位差是干净的——这正是 PLL 鉴相所需. */
void SigAnalysis_phasesAtBins(const uint16_t *buf, uint16_t n,
                              const uint16_t *bins, uint8_t nbins, int32_t *out_q31)
{
    const uint16_t nhalf = (uint16_t)(H_FFT_N / 2u);
    uint8_t s;

    prep_fft(buf, n);
    for (s = 0u; s < nbins; s++) {
        uint16_t kb = bins[s];
        out_q31[s] = (kb < nhalf)
            ? MathUtil_atan2_q31(((int32_t)s_fft[2u * kb])      << 16,
                                 ((int32_t)s_fft[2u * kb + 1u]) << 16)
            : 0;
    }
}
