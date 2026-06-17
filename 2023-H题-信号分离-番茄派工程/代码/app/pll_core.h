#ifndef PLL_CORE_H
#define PLL_CORE_H

#include <stdint.h>

/* ============================================================================
 *  再生法软件锁相环 — 纯 C 核心 (可离板单元测试; 目标端 pll.c 调用同一份)
 *
 *  结构: II 型数字 PLL (PI 环路), NCO = AD9959 的频率控制字 FTW.
 *        相位差只用 CPOW 在 build 时设一次初值; 锁相过程只调 FTW(频率),
 *        靠频率微调把相位误差拉到 0 并保持 —— 这正是抗独立晶振漂移的关键.
 *
 *  相位表示: 32 位归一化角, 2^32 = 360deg (与 DDS 相位累加器同一约定).
 *            两路相位作差用 int32 自然回绕 -> 天然得到 [-180,+180) 最短弧误差,
 *            绝不溢出 (这是相对旧版 kp*phase_q31 整型溢出的根本修复).
 *
 *  为什么是 II 型: 信号源晶振与 AD9959 的 25MHz 晶振相互独立, 必有 ppm 级频差.
 *            频差对 PLL 是"相位斜坡输入", 只有含积分项的 II 型环路才能做到
 *            稳态相位误差 = 0 (I 型会留下固定相位偏差). 积分项最终收敛到
 *            "抵消两晶振频差所需的 FTW 偏移".
 *
 *  增益用右移表示 (kp_shift / ki_shift), 而非乘法 —— 移位天然不溢出.
 * ========================================================================== */

typedef struct {
    uint32_t ftw_base;     /* 预调谐 FTW: 由粗测频率算得 (Recon_freqToFtw) */
    uint32_t ftw_out;      /* 最近一次输出的 FTW (= base + 比例 + 积分) */
    int64_t  integ_acc;    /* 全分辨率积分累加器(无死区): 每环 += perr.
                            * 用 int64 是为了让任意小的 perr 也能逐步累积,
                            * 从而真正消除稳态相位误差; 纯移位累加会有截断死区. */
    int32_t  freq_integ;   /* = integ_acc >> ki_shift, 限幅后的频差补偿 (FTW LSB) */
    int32_t  integ_limit;  /* 对 freq_integ 限幅 (FTW LSB), 须 >= 实际所需补偿范围 */
    int32_t  kp_shift;     /* 比例增益 = perr >> kp_shift */
    int32_t  ki_shift;     /* 积分增益 = integ_acc >> ki_shift */
    int32_t  last_perr;    /* 最近相位误差 (int32 归一化角; 失锁判据/诊断用) */
    uint8_t  locked;       /* 锁定标志: 连续多帧 |perr| 小于阈值 */
    uint8_t  lock_cnt;     /* 锁定计数 */
} pllcore_t;

/* 初始化. ftw_base 为预调谐频率字; kp_shift/ki_shift/integ_limit 见 pll.c 默认值. */
void PllCore_init(pllcore_t *p, uint32_t ftw_base,
                  int32_t kp_shift, int32_t ki_shift, int32_t integ_limit);

/* 频率粗测变化(信号源重新调频)时重设基准, 积分项保留以平滑过渡. */
void PllCore_retune(pllcore_t *p, uint32_t ftw_base);

/* 一次环路更新.
 *   phi_ref    : 原信号分量相位 (32 位归一化角, 来自 FFT/atan2)
 *   phi_fb     : 重建信号 A' 的实测相位 (来自第 2 路 ADC; 见 pll.c 说明)
 *   phi_target : 期望的 (ref - fb) 相位差 (发挥②相位可控; 0=同相)
 * 返回: 应施加到 DDS 的 FTW. 同时更新 p->last_perr / p->locked. */
uint32_t PllCore_update(pllcore_t *p, uint32_t phi_ref, uint32_t phi_fb,
                        uint32_t phi_target);

/* 锁定判据: 连续 PLLCORE_LOCK_FRAMES 帧 |perr| < 阈值 视为已锁.
 * 0x00B60B60 ≈ 360deg * 0x00B60B60 / 2^32 ≈ 1.0deg. */
#define PLLCORE_LOCK_THRESH   0x00B60B60
#define PLLCORE_LOCK_FRAMES   8u

#endif /* PLL_CORE_H */
