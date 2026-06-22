#include "pll.h"
#include "h_config.h"
#include "reconstruct.h"
#include "reconstruct_core.h"

/* 每路一个锁相环核心(状态在本模块内, h_app 只调接口) */
static pllcore_t s_core[H_NUM_SIGNALS];

void Pll_init(void)
{
    uint8_t i;
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        PllCore_init(&s_core[i], 0u,
                     PLL_KP_SHIFT, PLL_KI_SHIFT, PLL_INTEG_LIMIT);
    }
}

void Pll_retune(uint8_t idx, uint32_t freq_hz)
{
    if (idx >= (uint8_t)H_NUM_SIGNALS) { return; }
    PllCore_retune(&s_core[idx], Recon_freqToFtw(freq_hz, PLL_DDS_REF_HZ));
}

uint8_t Pll_track(uint8_t idx, int32_t phi_ref_q31, int32_t phi_fb_q31,
                  int32_t phi_target_q31)
{
    uint32_t ftw;

    if (idx >= (uint8_t)H_NUM_SIGNALS) { return 0u; }

    /* Q31 归一化角(±180°=±2^31) 直接 reinterpret 成 2^32=360° 的 uint32:
     * 二进制补码下 -180°..+180° 自然映射到 0..360°(如 -90°->0xC0000000=270°). */
    ftw = PllCore_update(&s_core[idx],
                         (uint32_t)phi_ref_q31,
                         (uint32_t)phi_fb_q31,
                         (uint32_t)phi_target_q31);

    /* 把锁相得到的 FTW 精确施加到 DDS (整数直写, 不走 float, 保亚-Hz 精度) */
    Reconstruct_setFtw(idx, ftw);

    return s_core[idx].locked;
}

uint8_t Pll_locked(uint8_t idx)
{
    return (idx < (uint8_t)H_NUM_SIGNALS) ? s_core[idx].locked : 0u;
}

void Pll_diag(uint8_t idx, int32_t *out_perr_q31, uint32_t *out_ftw, uint8_t *out_locked)
{
    if (idx >= (uint8_t)H_NUM_SIGNALS) { return; }
    if (out_perr_q31 != 0) { *out_perr_q31 = s_core[idx].last_perr; }
    if (out_ftw      != 0) { *out_ftw      = s_core[idx].ftw_out;   }
    if (out_locked   != 0) { *out_locked   = s_core[idx].locked;    }
}
