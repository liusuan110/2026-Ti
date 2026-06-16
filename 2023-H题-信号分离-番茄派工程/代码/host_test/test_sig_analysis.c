/* 离板验证 sig_analysis_core: 合成 A+B 信号 -> 参考FFT -> 算法核 -> 断言.
 * 在 PC 上跑: cc -std=c99 -I ../app -o test test_sig_analysis.c ../app/sig_analysis_core.c -lm && ./test
 * (核心算法与目标端共用同一份 sig_analysis_core.c; 此处用 float 参考FFT 代替目标端的 CMSIS q15 FFT) */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "sig_analysis_core.h"     /* 同时带入 h_config.h */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NFFT   ((int)H_FFT_N)
#define FS     ((double)H_ADC_FS_HZ)

/* 迭代基2 FFT(float, in-place, DIT) */
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

/* 三角波(归一化峰值~1): 奇次谐波 1/n^2, 交替符号 */
static double tri(double f, double t, double ph)
{
    double s = 0.0; int n; int sign = 1;
    for (n = 1; n <= 9; n += 2) {
        s += sign * (1.0 / (n * (double)n)) * sin(2.0 * M_PI * n * f * t + n * ph);
        sign = -sign;
    }
    return s * (8.0 / (M_PI * M_PI));
}

static double wave(int triangle, double f, double t, double ph)
{
    return triangle ? tri(f, t, ph) : sin(2.0 * M_PI * f * t + ph);
}

/* 合成 N 点 ADC 样本(DC 2048, A+B 叠加), 做 FFT -> 半谱幅度 uint16 -> 算法核 */
static void run_case(const char *name,
                     double fA, int triA, double ampA, double phA,
                     double fB, int triB, double ampB, double phB,
                     h_result_t *res)
{
    static float re[NFFT], im[NFFT];
    static uint16_t mag[NFFT / 2];
    int i, k;
    double mean = 0.0, mx = 0.0;

    for (i = 0; i < NFFT; i++) {
        double t = (double)i / FS;
        double x = 2048.0 + ampA * wave(triA, fA, t, phA) + ampB * wave(triB, fB, t, phB);
        re[i] = (float)x; im[i] = 0.0f;
    }
    for (i = 0; i < NFFT; i++) mean += re[i];
    mean /= NFFT;
    for (i = 0; i < NFFT; i++) re[i] -= (float)mean;   /* 去直流(对应目标端减2048) */

    fft(re, im, NFFT);

    for (k = 0; k < NFFT / 2; k++) {
        double mg = sqrt((double)re[k] * re[k] + (double)im[k] * im[k]);
        if (mg > mx) mx = mg;
    }
    for (k = 0; k < NFFT / 2; k++) {
        double mg = sqrt((double)re[k] * re[k] + (double)im[k] * im[k]);
        double v = (mx > 0.0) ? (mg * 20000.0 / mx) : 0.0;
        mag[k] = (uint16_t)(v + 0.5);
    }

    SigCore_analyze(mag, (uint16_t)(NFFT / 2), (uint32_t)H_ADC_FS_HZ, res);
    (void)name;
}

static const char *wname(h_wave_t w) { return (w == H_WAVE_SINE) ? "sine" : "tri "; }

static int check(const char *name,
                 uint32_t fexp0, h_wave_t wexp0, uint32_t fexp1, h_wave_t wexp1,
                 const h_result_t *res)
{
    uint32_t f0 = res->sig[0].freq_hz, f1 = res->sig[1].freq_hz;
    h_wave_t w0 = res->sig[0].wave,    w1 = res->sig[1].wave;
    long df0 = (long)f0 - (long)fexp0, df1 = (long)f1 - (long)fexp1;
    int ok_f = (labs(df0) <= 800) && (labs(df1) <= 800);
    int ok_w = (w0 == wexp0) && (w1 == wexp1);
    int ok = ok_f && ok_w;
    printf("[%-22s] A=%6u/%s (exp %6u/%s)  B=%6u/%s (exp %6u/%s)  dF=%+ld/%+ld  -> %s\n",
           name, f0, wname(w0), fexp0, wname(wexp0), f1, wname(w1), fexp1, wname(wexp1),
           df0, df1, ok ? "PASS" : "FAIL");
    return ok;
}

int main(void)
{
    h_result_t r;
    int pass = 0, total = 0;

    printf("fs=%.0f Hz, N=%d, fRes=%.1f Hz\n", FS, NFFT, FS / NFFT);

    run_case("基本:50k+100k 正弦", 50000, 0, 600, 0.3,  100000, 0, 600, 1.1, &r);
    total++; pass += check("50k sine + 100k sine", 50000, H_WAVE_SINE, 100000, H_WAVE_SINE, &r);

    run_case("fB=3fA:10k+30k 正弦", 10000, 0, 600, 0.0,  30000, 0, 600, 0.5, &r);
    total++; pass += check("10k sine + 30k sine", 10000, H_WAVE_SINE, 30000, H_WAVE_SINE, &r);

    run_case("混合:15k三角+40k正弦", 15000, 1, 600, 0.2,  40000, 0, 600, 0.9, &r);
    total++; pass += check("15k tri + 40k sine", 15000, H_WAVE_TRIANGLE, 40000, H_WAVE_SINE, &r);

    run_case("难:20k三角+60k三角", 20000, 1, 600, 0.0,  60000, 1, 600, 0.4, &r);
    total++; pass += check("20k tri + 60k tri", 20000, H_WAVE_TRIANGLE, 60000, H_WAVE_TRIANGLE, &r);

    run_case("离bin:52.3k+77.7k 正弦", 52300, 0, 600, 0.7, 77700, 0, 600, 0.1, &r);
    total++; pass += check("52.3k + 77.7k sine", 52300, H_WAVE_SINE, 77700, H_WAVE_SINE, &r);

    printf("\n==== %d/%d PASS ====\n", pass, total);
    return (pass == total) ? 0 : 1;
}
