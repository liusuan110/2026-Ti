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
        res->sig[s].valid   = 1u;
    }

    /* fB ≈ 3*fA: A 的 3 次谐波落在 B 的基波上被污染 -> 改用 5 次谐波重判 A */
    {
        int diff = (int)k1 - 3 * (int)k0;
        if (diff < 0) { diff = -diff; }
        if (diff <= 1) { res->sig[0].wave = classify_harm(mag, k0, nhalf, 5u); }
    }
}
