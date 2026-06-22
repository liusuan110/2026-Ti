#include "sig_analysis_core.h"

/* [lo,hi] 内幅度最大 bin */
static uint16_t peak_in(const uint16_t *mag, uint16_t lo, uint16_t hi)
{
    uint16_t k, best = lo, bv = 0u;
    for (k = lo; k <= hi; k++) {
        if (mag[k] > bv) { bv = mag[k]; best = k; }
    }
    return best;
}

/* [lo,hi] 内、排除 [k0-g, k0+g] 后的最大 bin (找第二峰) */
static uint16_t peak_excl(const uint16_t *mag, uint16_t lo, uint16_t hi, uint16_t k0, uint16_t g)
{
    uint16_t k, best = lo, bv = 0u;
    uint8_t found = 0u;
    for (k = lo; k <= hi; k++) {
        if ((k <= (uint16_t)(k0 + g)) && ((uint16_t)(k + g) >= k0)) {
            continue;  /* |k-k0| <= g, 跳过主峰邻域 */
        }
        if ((found == 0u) || (mag[k] > bv)) { bv = mag[k]; best = k; found = 1u; }
    }
    return best;
}

/* 抛物线插值: 返回峰相对 k 的亚 bin 偏移 [-0.5, 0.5] */
static float parab(const uint16_t *mag, uint16_t k)
{
    float a = (float)mag[k - 1u];
    float b = (float)mag[k];
    float c = (float)mag[k + 1u];
    float den = a - 2.0f * b + c;
    float d = (den != 0.0f) ? (0.5f * (a - c) / den) : 0.0f;
    if (d >  0.5f) { d =  0.5f; }
    if (d < -0.5f) { d = -0.5f; }
    return d;
}

/* 基波/谐波 三bin 能量和 */
static uint32_t band3(const uint16_t *mag, uint16_t k)
{
    return (uint32_t)mag[k - 1u] + (uint32_t)mag[k] + (uint32_t)mag[k + 1u];
}

/* 用 m 次谐波判波形: 基波 > m次谐波 * 阈值 => 正弦, 否则三角 */
static h_wave_t classify_harm(const uint16_t *mag, uint16_t kf, uint16_t nhalf, uint16_t m)
{
    uint16_t km = (uint16_t)(m * kf);
    uint32_t p1, pm;
    if ((uint16_t)(km + 1u) >= nhalf) { return H_WAVE_SINE; }  /* 看不到该谐波 -> 视为正弦 */
    p1 = band3(mag, kf);
    pm = band3(mag, km);
    if (pm == 0u) { return H_WAVE_SINE; }
    return (p1 > pm * SIGCORE_HARM_THRESH) ? H_WAVE_SINE : H_WAVE_TRIANGLE;
}

void SigCore_analyze(const uint16_t *mag, uint16_t nhalf, uint32_t fs_hz, h_result_t *res)
{
    uint16_t nfft = (uint16_t)(nhalf << 1);
    uint16_t lo = (uint16_t)((uint64_t)H_FREQ_MIN_HZ * nfft / fs_hz);
    uint16_t hi = (uint16_t)((uint64_t)H_FREQ_MAX_HZ * nfft / fs_hz);
    uint16_t k0, k1, ks[H_NUM_SIGNALS], s;

    if (lo < 2u) { lo = 2u; }
    if (hi > (uint16_t)(nhalf - 2u)) { hi = (uint16_t)(nhalf - 2u); }

    k0 = peak_in(mag, lo, hi);
    k1 = peak_excl(mag, lo, hi, k0, 2u);
    if (k1 < k0) { uint16_t t = k0; k0 = k1; k1 = t; }   /* 升序: 低频在前 */
    ks[0] = k0;
    ks[1] = k1;

    for (s = 0u; s < (uint16_t)H_NUM_SIGNALS; s++) {
        uint16_t kf = ks[s];
        float frac = parab(mag, kf);
        res->sig[s].freq_hz = (uint32_t)(((float)kf + frac) * (float)fs_hz / (float)nfft + 0.5f);
        res->sig[s].amp     = mag[kf];
        res->sig[s].wave    = classify_harm(mag, kf, nhalf, 3u);
        res->sig[s].phase_q31 = 0;
        res->sig[s].bin     = kf;     /* ANA-1: 暴露整数峰 bin 供相位 atan2 复用 */
        res->sig[s].valid   = 1u;
        res->sig[s].overlap = 0u;
    }

    /* fB ≈ 3*fA: A 的 3 次谐波正落在 B 的基波 bin 上 (ANA-3). */
    {
        int diff = (int)k1 - 3 * (int)k0;
        if (diff < 0) { diff = -diff; }
        if (diff <= 1) {
            uint16_t est;        /* A 三次谐波在重叠 bin 处的峰值估计 */
            uint16_t k3;

            /* (1) A 改用 5 次谐波重判(5*k0 不与 B 重叠) */
            res->sig[0].wave    = classify_harm(mag, k0, nhalf, 5u);
            res->sig[1].overlap = 1u;

            /* (2) 估 A 的 3 次谐波幅度: 三角波约为基波的 1/9, 正弦≈0 */
            est = (res->sig[0].wave == H_WAVE_TRIANGLE) ? (uint16_t)(mag[k0] / 9u) : 0u;

            /* (3) 去污 B 幅度: 重叠 bin 幅度减去 A 三次谐波贡献 */
            res->sig[1].amp = (mag[k1] > est) ? (uint16_t)(mag[k1] - est) : 0u;

            /* (4) 用去污后的 B 基波能量重判 B 波形(对照 B 自身 3 次谐波 3*k1).
             * 注意 fB=3fA 时, B 的 3 次谐波检测位 3*k1=9*k0 又撞上 A 的 9 次谐波,
             * 故 p1 扣 A 的 3 次谐波(≈基波/9), p3 扣 A 的 9 次谐波(≈基波/81). */
            k3 = (uint16_t)(3u * k1);
            if ((uint16_t)(k3 + 1u) < nhalf) {
                uint16_t est9 = (res->sig[0].wave == H_WAVE_TRIANGLE) ? (uint16_t)(mag[k0] / 81u) : 0u;
                uint32_t estB1 = (uint32_t)est  * 2u;      /* A 3 次谐波三-bin 贡献 ≈ 2×峰值(Hann) */
                uint32_t estB3 = (uint32_t)est9 * 2u;      /* A 9 次谐波三-bin 贡献 */
                uint32_t p1   = band3(mag, k1);
                uint32_t p3   = band3(mag, k3);
                p1 = (p1 > estB1) ? (p1 - estB1) : 0u;     /* 去污后的 B 基波三-bin 能量 */
                p3 = (p3 > estB3) ? (p3 - estB3) : 0u;     /* 去污后的 B 三次谐波三-bin 能量 */
                if (p3 == 0u) { res->sig[1].wave = H_WAVE_SINE; }
                else { res->sig[1].wave = (p1 > p3 * SIGCORE_HARM_THRESH)
                                          ? H_WAVE_SINE : H_WAVE_TRIANGLE; }
            }
            /* k3 越界则保留原判(已用 k1, 受污染, 但无更高谐波可依) */
        }
    }
}

/* ANA-2: 块归一化移位. 把峰值左移到落入 (0.5,1.0] 满量程, 不溢出 q15. */
int32_t SigCore_normShift(uint32_t maxabs)
{
    int32_t sh = 0;
    if (maxabs == 0u) { return 0; }
    while ((sh < 15) && (((uint32_t)maxabs << (uint32_t)(sh + 1)) <= 32767u)) {
        sh++;
    }
    return sh;
}

/* 频率吸附到最近 H_FREQ_STEP_HZ 网格, 夹到 [H_FREQ_MIN_HZ, H_FREQ_MAX_HZ]. */
uint32_t SigCore_snapFreq(uint32_t f_hz)
{
    const uint32_t step = (uint32_t)H_FREQ_STEP_HZ;
    const uint32_t lo   = (uint32_t)H_FREQ_MIN_HZ;
    const uint32_t hi   = (uint32_t)H_FREQ_MAX_HZ;
    uint32_t snapped;
    if (f_hz <= lo) { return lo; }
    if (f_hz >= hi) { return hi; }
    snapped = ((f_hz + step / 2u) / step) * step;   /* 四舍五入到 step 倍数 */
    if (snapped < lo) { snapped = lo; }
    if (snapped > hi) { snapped = hi; }
    return snapped;
}
