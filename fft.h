/*
 * fft.h
 * 32 点原位基-2 DIT FFT (16位定点)
 *
 * RAM 占用: real[32] + imag[32] = 128 字节
 * 旋转因子表存 Flash: sin_table[16] = 32 字节
 */

#ifndef __FFT_H__
#define __FFT_H__

#include "config.h"

/*
 * 原位 32 点 FFT
 * real[]: 输入实部 (ADC值, 会被原位覆盖)
 * imag[]: 输入虚部 (应初始化为0, 会被原位覆盖)
 * 输出: real[], imag[] 为频域结果
 */
void fft32(int16_t real[], int16_t imag[]);

/*
 * 计算各频点的模值 (近似值: |real| + |imag|)
 * real[], imag[]: FFT 输出
 * mag[]: 输出模值数组, 长度 FFT_N/2 = 16
 * 只计算前 N/2 个点 (对称性)
 */
void fft_magnitude(const int16_t real[], const int16_t imag[], uint16_t mag[]);

#endif /* __FFT_H__ */
