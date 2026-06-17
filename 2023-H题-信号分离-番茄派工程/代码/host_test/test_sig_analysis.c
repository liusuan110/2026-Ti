/* 离板验证 sig_analysis_core(含汉宁窗): 合成 A+B -> 汉宁窗 -> 参考FFT -> 算法核 -> 断言.
 * cc -std=c99 -I ../app -o test test_sig_analysis.c ../app/sig_analysis_core.c -lm && ./test
 * 核心算法与目标端共用 sig_analysis_core.c + hann1024.h; 此处用 float 参考FFT 代替 CMSIS q15 FFT. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "sig_analysis_core.h"   /* 带入 h_config.h */
#include "hann1024.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define NFFT ((int)H_FFT_N)
#define FS   ((double)H_ADC_FS_HZ)

static void fft(float *re, float *im, int n)
{
    int i, j, m, k;
    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (m = 2; m <= n; m <<= 1) {
        double ang = -2.0 * M_PI / m;
        float wr = (float)cos(ang), wi = (float)sin(ang);
        for (k = 0; k < n; k += m) {
            float cr = 1.0f, ci = 0.0f;
            for (j = 0; j < m / 2; j++) {
                int a = k + j, b = k + j + m / 2;
                float tr = cr * re[b] - ci * im[b];
                float ti = cr * im[b] + ci * re[b];
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr;        im[a] += ti;
                float ncr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = ncr;
            }
        }
    }
}

static double tri(double f, double t, double ph)
{
    double s = 0.0; int n; int sign = 1;
    for (n = 1; n <= 9; n += 2) { s += sign * (1.0 / (n * (double)n)) * sin(2.0 * M_PI * n * f * t + n * ph); sign = -sign; }
    return s * (8.0 / (M_PI * M_PI));
}
static double wave(int triangle, double f, double t, double ph)
{ return triangle ? tri(f, t, ph) : sin(2.0 * M_PI * f * t + ph); }

static void run_case(double fA, int triA, double ampA, double phA,
                     double fB, int triB, double ampB, double phB, h_result_t *res)
{
    static float re[NFFT], im[NFFT];
    static uint16_t mag[NFFT / 2];
    int i, k; double mean = 0.0, mx = 0.0;
    for (i = 0; i < NFFT; i++) {
        double t = (double)i / FS;
        re[i] = (float)(2048.0 + ampA * wave(triA, fA, t, phA) + ampB * wave(triB, fB, t, phB));
        im[i] = 0.0f;
    }
    for (i = 0; i < NFFT; i++) mean += re[i];
    mean /= NFFT;
    for (i = 0; i < NFFT; i++) re[i] = (float)((re[i] - mean) * (hann1024[i] / 32767.0)); /* 去直流 + 汉宁窗 */
    fft(re, im, NFFT);
    for (k = 0; k < NFFT / 2; k++) { double mg = sqrt((double)re[k]*re[k] + (double)im[k]*im[k]); if (mg > mx) mx = mg; }
    for (k = 0; k < NFFT / 2; k++) {
        double mg = sqrt((double)re[k]*re[k] + (double)im[k]*im[k]);
        mag[k] = (uint16_t)((mx > 0.0 ? mg * 20000.0 / mx : 0.0) + 0.5);
    }
    SigCore_analyze(mag, (uint16_t)(NFFT / 2), (uint32_t)H_ADC_FS_HZ, res);
}

static const char *wn(h_wave_t w) { return (w == H_WAVE_SINE) ? "sine" : "tri "; }

static int check(const char *name, uint32_t fe0, h_wave_t we0, uint32_t fe1, h_wave_t we1, const h_result_t *r)
{
    long d0 = (long)r->sig[0].freq_hz - (long)fe0, d1 = (long)r->sig[1].freq_hz - (long)fe1;
    int ok = (labs(d0) <= 800) && (labs(d1) <= 800) && (r->sig[0].wave == we0) && (r->sig[1].wave == we1);
    printf("[%-20s] A=%6u/%s B=%6u/%s  dF=%+4ld/%+4ld  -> %s\n",
           name, r->sig[0].freq_hz, wn(r->sig[0].wave), r->sig[1].freq_hz, wn(r->sig[1].wave), d0, d1, ok ? "PASS" : "FAIL");
    return ok;
}

/* ANA-1: 校验暴露的峰值 bin 与期望一致(fs=1.024M,N=1024 => bin≈f/1000) */
static int check_bin(const char *name, uint16_t b0e, uint16_t b1e, const h_result_t *r)
{
    int ok = (abs((int)r->sig[0].bin - (int)b0e) <= 1) && (abs((int)r->sig[1].bin - (int)b1e) <= 1);
    printf("[%-20s] bin A=%u(期望%u) B=%u(期望%u) -> %s\n",
           name, r->sig[0].bin, b0e, r->sig[1].bin, b1e, ok ? "PASS" : "FAIL");
    return ok;
}

/* ANA-3: 校验重叠标志 + B 波形(幅度悬殊时, 未去污会把三角 B 误判正弦) */
static int check_overlap(const char *name, h_wave_t we1, const h_result_t *r)
{
    int ok = (r->sig[1].overlap == 1u) && (r->sig[1].wave == we1);
    printf("[%-20s] overlap=%u B波形=%s(期望%s) -> %s\n",
           name, r->sig[1].overlap, wn(r->sig[1].wave), wn(we1), ok ? "PASS" : "FAIL");
    return ok;
}

/* ANA-2: 校验块归一化移位把峰值拉到 (0.5,1.0] 满量程且不溢出 */
static int test_normshift(void)
{
    uint32_t cases[] = {620u, 2048u, 100u, 1u, 30000u, 0u};
    int i, ok = 1;
    printf("--- ANA-2 SigCore_normShift ---\n");
    for (i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
        uint32_t m = cases[i];
        int32_t sh = SigCore_normShift(m);
        long peak = (long)m << sh;
        int good;
        if (m == 0u) { good = (sh == 0); }
        else { good = (peak <= 32767) && ((peak > 16383) || (sh >= 15)); }  /* 落入(0.5,1.0]或封顶 */
        printf("  maxabs=%5u -> sh=%2d 峰=%5ld  %s\n", m, sh, peak, good ? "ok" : "BAD");
        ok = ok && good;
    }
    printf("  -> %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

int main(void)
{
    h_result_t r; int pass = 0, total = 0;
    printf("fs=%.0f N=%d fRes=%.1f (汉宁窗)\n", FS, NFFT, FS / NFFT);
    run_case(50000, 0, 600, 0.3, 100000, 0, 600, 1.1, &r); total++; pass += check("50k+100k sine", 50000, H_WAVE_SINE, 100000, H_WAVE_SINE, &r);
    total++; pass += check_bin("50k+100k bin", 50u, 100u, &r);   /* ANA-1 */
    run_case(10000, 0, 600, 0.0, 30000, 0, 600, 0.5, &r);  total++; pass += check("10k+30k sine(3x)", 10000, H_WAVE_SINE, 30000, H_WAVE_SINE, &r);
    run_case(15000, 1, 600, 0.2, 40000, 0, 600, 0.9, &r);  total++; pass += check("15k tri+40k sine", 15000, H_WAVE_TRIANGLE, 40000, H_WAVE_SINE, &r);
    run_case(20000, 1, 600, 0.0, 60000, 1, 600, 0.4, &r);  total++; pass += check("20k+60k tri(3x)", 20000, H_WAVE_TRIANGLE, 60000, H_WAVE_TRIANGLE, &r);
    run_case(52300, 0, 600, 0.7, 77700, 0, 600, 0.1, &r);  total++; pass += check("52.3k+77.7k off-bin", 52300, H_WAVE_SINE, 77700, H_WAVE_SINE, &r);

    /* ANA-3 谐波重叠去污关键案例: A三角(大) + B正弦(小, 5:1). A的3次谐波淹没B基波,
     * 未去污时 B 被误判三角; 去污后正确判正弦. (这是审计 ANA-3"幅度悬殊"的真实可解场景) */
    run_case(20000, 1, 1500, 0.0, 60000, 0, 300, 0.7, &r); total++; pass += check_overlap("20k tri+60k sine(去污)", H_WAVE_SINE, &r);
    /* A三角 + B三角(轻度差异), 去污不应破坏三角判定 + 标 overlap */
    run_case(20000, 1, 800, 0.0, 60000, 1, 600, 0.4, &r);  total++; pass += check_overlap("20k tri+60k tri(3x)", H_WAVE_TRIANGLE, &r);

    total++; pass += test_normshift();   /* ANA-2 */

    printf("\n==== %d/%d PASS ====\n", pass, total);
    return (pass == total) ? 0 : 1;
}
