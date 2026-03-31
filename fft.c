/*
 * fft.c
 * 32 点原位基-2 DIT FFT (16位定点)
 *
 * 旋转因子: W_N^k = cos(2*pi*k/N) - j*sin(2*pi*k/N)
 * 使用 Q14 定点格式: 1.0 -> 16384
 *
 * 每级蝶形运算后右移1位防止溢出
 */

#include "fft.h"

/* Q14 格式的 sin 表 (完整周期): sin(2*pi*k/32), k = 0..31 */
/* 值 = round(sin(2*pi*k/32) * 16384), 存 Flash 不占 RAM */
static const int16_t sin_table[FFT_N] = {
        0,   3196,   6270,   9102,  11585,  13623,  15137,  16069,
    16384,  16069,  15137,  13623,  11585,   9102,   6270,   3196,
        0,  -3196,  -6270,  -9102, -11585, -13623, -15137, -16069,
   -16384, -16069, -15137, -13623, -11585,  -9102,  -6270,  -3196
};

/* cos(2*pi*k/N) = sin(2*pi*k/N + pi/2) = sin_table[(k + N/4) % N] */
static int16_t fft_cos(uint16_t k)
{
    return sin_table[(k + FFT_N / 4) % FFT_N];
}

static int16_t fft_sin(uint16_t k)
{
    return sin_table[k % FFT_N];
}

/* 位反转排列 (5 位) */
static uint16_t bit_reverse(uint16_t x)
{
    uint16_t result = 0;
    uint16_t i;
    for (i = 0; i < FFT_LOG2N; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

/*
 * 32 点原位 FFT
 * 输入: real[] = 采样数据, imag[] = 全0
 * 输出: real[], imag[] = 频域
 */
void fft32(int16_t real[], int16_t imag[])
{
    uint16_t i, j, k;
    uint16_t step, half_step;
    uint16_t tw_idx, jh;
    int16_t tr, ti;
    int16_t wr, wi;

    /* 步骤1: 位反转重排 */
    for (i = 0; i < FFT_N; i++) {
        j = bit_reverse(i);
        if (j > i) {
            /* 交换 real */
            tr = real[i];
            real[i] = real[j];
            real[j] = tr;
            /* 交换 imag */
            ti = imag[i];
            imag[i] = imag[j];
            imag[j] = ti;
        }
    }

    /* 步骤2: 蝶形运算, 共 log2(N) = 5 级 */
    for (step = 2; step <= FFT_N; step <<= 1) {
        half_step = step >> 1;

        for (k = 0; k < half_step; k++) {
            /* 旋转因子 W = cos - j*sin */
            /* 索引 = k * (N / step) */
            tw_idx = k * (FFT_N / step);
            wr =  fft_cos(tw_idx);      /*  cos(2*pi*tw_idx/N) in Q14 */
            wi = -fft_sin(tw_idx);      /* -sin(2*pi*tw_idx/N) in Q14 */

            for (j = k; j < FFT_N; j += step) {
                jh = j + half_step;

                /* 蝶形: t = W * x[jh] */
                /* Q14 乘法: (a * b) >> 14 */
                tr = (int16_t)(((int32_t)wr * real[jh] - (int32_t)wi * imag[jh]) >> 14);
                ti = (int16_t)(((int32_t)wr * imag[jh] + (int32_t)wi * real[jh]) >> 14);

                real[jh] = real[j] - tr;
                imag[jh] = imag[j] - ti;
                real[j]  = real[j] + tr;
                imag[j]  = imag[j] + ti;
            }
        }

        /* 每级结束后全体右移1位, 防止 int16 溢出 */
        for (i = 0; i < FFT_N; i++) {
            real[i] >>= 1;
            imag[i] >>= 1;
        }
    }
}

/*
 * 计算模值 (近似: |re| + |im|, 省去 sqrt)
 * 只输出前 N/2 个频点
 */
void fft_magnitude(const int16_t real[], const int16_t imag[], uint16_t mag[])
{
    uint16_t i;
    int16_t re, im;

    for (i = 0; i < FFT_N / 2; i++) {
        re = real[i];
        im = imag[i];
        if (re < 0) re = -re;
        if (im < 0) im = -im;
        mag[i] = (uint16_t)(re + im);
    }
}
