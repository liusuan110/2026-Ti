#include "pll_core.h"

void PllCore_init(pllcore_t *p, uint32_t ftw_base,
                  int32_t kp_shift, int32_t ki_shift, int32_t integ_limit)
{
    p->ftw_base    = ftw_base;
    p->ftw_out     = ftw_base;
    p->integ_acc   = 0;
    p->freq_integ  = 0;
    p->integ_limit = integ_limit;
    p->kp_shift    = kp_shift;
    p->ki_shift    = ki_shift;
    p->last_perr   = 0;
    p->locked      = 0u;
    p->lock_cnt    = 0u;
}

void PllCore_retune(pllcore_t *p, uint32_t ftw_base)
{
    /* 频率粗测跳变: 换基准, 但保留积分累加器(它代表两晶振 ppm 频差,
     * 与目标频率弱相关, 保留可平滑过渡). 重置锁定计数. */
    p->ftw_base = ftw_base;
    p->lock_cnt = 0u;
    p->locked   = 0u;
}

uint32_t PllCore_update(pllcore_t *p, uint32_t phi_ref, uint32_t phi_fb,
                        uint32_t phi_target)
{
    int32_t perr;
    int32_t prop;
    int32_t corr;
    int64_t acc_lim;

    /* --- 相位检测器 ---
     * (ref + target) - fb, 全部按 uint32 相加减后再转 int32.
     * int32 自然回绕等价于"对 2^32(=360deg) 取模再折到 [-180,+180)",
     * 即最短弧相位误差. perr 上限 ±2^31, 不会溢出. */
    perr = (int32_t)((phi_ref + phi_target) - phi_fb);
    p->last_perr = perr;

    /* --- 环路滤波器 (PI) ---
     * 积分: 全分辨率累加(int64, 无死区) -> 右移得频差补偿(FTW LSB).
     * 通过限制累加器幅度来实现对 freq_integ 的抗饱和(acc_lim = limit<<ki_shift). */
    p->integ_acc += (int64_t)perr;
    acc_lim = (int64_t)p->integ_limit << p->ki_shift;
    if (p->integ_acc >  acc_lim) { p->integ_acc =  acc_lim; }
    if (p->integ_acc < -acc_lim) { p->integ_acc = -acc_lim; }
    p->freq_integ = (int32_t)(p->integ_acc >> p->ki_shift);

    /* 比例: 算术右移做增益 -> 不可能溢出 (perr>>k 必落在 perr 范围内). */
    prop = (perr >> p->kp_shift);

    /* --- NCO: FTW = 预调谐 + 比例 + 积分 ---
     * corr = prop + integ 均为有界 int32, 之和仍在 int32 内;
     * 与 ftw_base 用 uint32 相加(模 2^32 自然回绕, FTW 本就是模 2^32 量). */
    corr        = prop + p->freq_integ;
    p->ftw_out  = p->ftw_base + (uint32_t)corr;

    /* --- 锁定判据: 用区间比较避免 abs(INT32_MIN) 的 UB --- */
    if ((perr > -PLLCORE_LOCK_THRESH) && (perr < PLLCORE_LOCK_THRESH)) {
        if (p->lock_cnt < 255u) { p->lock_cnt++; }
    } else {
        p->lock_cnt = 0u;
    }
    p->locked = (p->lock_cnt >= PLLCORE_LOCK_FRAMES) ? 1u : 0u;

    return p->ftw_out;
}
