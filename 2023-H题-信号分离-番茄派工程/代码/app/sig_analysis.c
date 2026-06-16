#include "sig_analysis.h"
#include "mathacl_util.h"
#include "arm_math.h"
#include "arm_const_structs.h"

/* 大缓冲放全局/静态, 绝不放栈上 (吸取 SHR 工程"爆栈使DA波形出问题"的教训) */
static q15_t s_fft[2U * H_FFT_N];   /* 交错复数, in-place FFT in/out (4KB) */
static q15_t s_mag[H_FFT_N];        /* 幅度谱 (2KB) */

void SigAnalysis_init(void)
{
    /* 预留: 窗函数表等 */
}

static uint16_t find_peak(uint16_t lo, uint16_t hi)
{
    uint16_t k, best = lo;
    q15_t bv = 0;
    for (k = lo; k <= hi; k++) {
        if (s_mag[k] > bv) { bv = s_mag[k]; best = k; }
    }
    return best;
}

uint8_t SigAnalysis_run(const uint16_t *buf, uint16_t n, h_result_t *res)
{
    uint16_t i, k0, k1, lo, hi, s;
    uint16_t nn = (n < (uint16_t)H_FFT_N) ? n : (uint16_t)H_FFT_N;
    uint16_t ks[H_NUM_SIGNALS];

    /* 1) 转 q15 交错复数, 去直流(减 2048), 放大到 q15 量级 */
    for (i = 0U; i < nn; i++) {
        s_fft[2U * i]      = (q15_t)(((int32_t)buf[i] - 2048) << 4);
        s_fft[2U * i + 1U] = 0;
    }
    for (; i < (uint16_t)H_FFT_N; i++) {
        s_fft[2U * i]      = 0;
        s_fft[2U * i + 1U] = 0;
    }

    /* 2) 定点 FFT (CMSIS-DSP, M0 定点) + 求模 */
    arm_cfft_q15(&arm_cfft_sR_q15_len1024, s_fft, 0, 1);
    arm_cmplx_mag_q15(s_fft, s_mag, H_FFT_N);

    /* 3) 在 [10kHz .. 100kHz] 对应 bin 范围内寻两个峰 */
    lo = (uint16_t)(H_FREQ_MIN_HZ / H_FREQ_RES_HZ);
    hi = (uint16_t)(H_FREQ_MAX_HZ / H_FREQ_RES_HZ);
    if (hi > ((uint16_t)H_FFT_N / 2U - 1U)) { hi = (uint16_t)H_FFT_N / 2U - 1U; }

    k0 = find_peak(lo, hi);
    {   /* 抑制 k0 邻域(±2)再找第二峰 */
        q15_t save[5];
        uint16_t g, kk;
        for (g = 0U; g < 5U; g++) {
            kk = (uint16_t)(k0 + g - 2U);
            save[g] = s_mag[kk];
            s_mag[kk] = 0;
        }
        k1 = find_peak(lo, hi);
        for (g = 0U; g < 5U; g++) {
            kk = (uint16_t)(k0 + g - 2U);
            s_mag[kk] = save[g];
        }
    }
    if (k1 < k0) { uint16_t t = k0; k0 = k1; k1 = t; }   /* 升序: 低频在前 */
    ks[0] = k0;
    ks[1] = k1;

    /* 4) 填两路结果 */
    for (s = 0U; s < (uint16_t)H_NUM_SIGNALS; s++) {
        uint16_t kf = ks[s];
        uint16_t h3 = (uint16_t)(3U * kf);
        res->sig[s].freq_hz = (uint32_t)kf * H_FREQ_RES_HZ;
        res->sig[s].amp     = (uint16_t)s_mag[kf];

        /* 相位: 峰值 bin 的复数 atan2 (MATHACL). 放大到高位供定点 atan2;
         * 精确 Q 标定留待上板. 约定: atan2(X=实部, Y=虚部) */
        res->sig[s].phase_q31 = MathUtil_atan2_q31(
            ((int32_t)s_fft[2U * kf])      << 16,
            ((int32_t)s_fft[2U * kf + 1U]) << 16);

        /* 波形: 3 次谐波能量远小于基波 -> 正弦; 否则三角 (阈值 8x 需上板标定) */
        if ((h3 < ((uint16_t)H_FFT_N / 2U)) &&
            ((int32_t)s_mag[kf] > ((int32_t)s_mag[h3] * 8))) {
            res->sig[s].wave = H_WAVE_SINE;
        } else {
            res->sig[s].wave = H_WAVE_TRIANGLE;
        }
        res->sig[s].valid = 1U;
    }
    return (uint8_t)H_NUM_SIGNALS;
}
