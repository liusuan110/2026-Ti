#include "reconstruct_core.h"

uint32_t Recon_freqToFtw(uint32_t freq_hz, uint32_t sysclk_hz)
{
    uint64_t num;
    if (sysclk_hz == 0u) { return 0u; }
    num = ((uint64_t)freq_hz << 32) + (uint64_t)(sysclk_hz >> 1);  /* +半除数 = 四舍五入 */
    return (uint32_t)(num / (uint64_t)sysclk_hz);
}

uint16_t Recon_angleQ31ToCpow(int32_t angle_q31)
{
    /* 逻辑右移 18 位: 32-18=14 位结果天然落在 0..16383, 且按 360° 取模 */
    return (uint16_t)(((uint32_t)angle_q31 >> 18) & 0x3FFFu);
}

uint16_t Recon_degToCpow(int32_t deg)
{
    int64_t raw = ((int64_t)deg * 16384 + 180) / 360;   /* +180 = 四舍五入 */
    return (uint16_t)((uint32_t)raw & 0x3FFFu);
}
