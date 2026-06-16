#ifndef RECONSTRUCT_CORE_H
#define RECONSTRUCT_CORE_H

#include <stdint.h>

/* AD9959 数值换算(纯 C, 可离板单元测试; 目标端 reconstruct.c 调用同一份). */

/* 频率(Hz) -> 32 位频率控制字 FTW = round(freq * 2^32 / sysclk).
 * 番茄派 AD9959: sysclk = 500MHz(25MHz 晶振 ×20), 与驱动 ACC_FRE_FACTOR=8.589934592 等价. */
uint32_t Recon_freqToFtw(uint32_t freq_hz, uint32_t sysclk_hz);

/* MATHACL 归一化角(Q31, 满量程对应 ±180°: 0x40000000=90°)
 *   -> AD9959 14 位相位字(0..16383 = 0..360°).
 * = (uint32)q31 >> 18; 自然对 360° 取模(负角折成对应正角, 如 -90°->270°=12288). */
uint16_t Recon_angleQ31ToCpow(int32_t angle_q31);

/* 角度(度) -> 14 位相位字(发挥②相位差用): round(deg/360 * 16384), 对 360° 取模. */
uint16_t Recon_degToCpow(int32_t deg);

#endif /* RECONSTRUCT_CORE_H */
