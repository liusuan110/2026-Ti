#include "h_app.h"
#include "h_config.h"
#include "sig_acq.h"
#include "sig_analysis.h"
#include "reconstruct.h"
#include "pll.h"
#include "mathacl_util.h"
#include "Tick.h"

#define H_CAP_N   ((uint16_t)H_FFT_N)

/* 固定控制周期(ms). SYS-4: 反馈环路控制间隔须固定(培训3 PPT 要点).
 * ★须 >= 单次 采集+FFT+反馈采集 的最长处理时间, 使实际 dt 恒等于本值(固定不抖).
 * 锁相增益 PLL_*_SHIFT 按此 dt 整定 —— 上板实测处理耗时后, 设好本值, 再用
 * host_test/test_pll.c 以该 dt 重整定增益. 默认 5ms 为整定假设值. */
#define H_LOOP_PERIOD_MS   5u

/* 大缓冲全局, 不放栈; 4 字节对齐(ADC DMA 按 32 位字写入) */
static uint16_t   s_buf[H_CAP_N]    __attribute__((aligned(4)));  /* 混合信号 C */
static uint16_t   s_fb_buf[H_CAP_N] __attribute__((aligned(4)));  /* 反馈 A'+B' (第2路ADC) */
static uint32_t   s_next_tick = 0u;               /* 下一控制周期边界(SysTick ms) */
static h_result_t s_res;                          /* 对 C 的分析结果 */
static int32_t    s_fb_phase[H_NUM_SIGNALS];      /* 重建信号 A'/B' 的实测相位(反馈) */
static int32_t    s_target[H_NUM_SIGNALS];        /* 目标相位差(发挥②, 0=同相) */
static uint32_t   s_built_freq[H_NUM_SIGNALS];    /* 已建链的频率, 用于判频率跳变 */
static uint8_t    s_built = 0U;

/* 分析结果是否可用作建链: 有效标志 + 频率落在题目范围 */
static uint8_t result_ok(const h_sig_t *s)
{
    return (uint8_t)(s->valid != 0u &&
                     s->freq_hz >= (uint32_t)H_FREQ_MIN_HZ &&
                     s->freq_hz <= (uint32_t)H_FREQ_MAX_HZ);
}

/* ----------------------------------------------------------------------------
 *  测量重建信号 A'/B' 的相位作为锁相反馈量.
 *  A'+B' 由第 2 路 ADC(ADC1)与 C 近同步采到 s_fb_buf(SigAcq_captureDual),
 *  这里对其做 窗+FFT, 在两路已知 bin(取自对 C 的分析 s_res.sig[i].bin)处 atan2.
 *  ★上板项(见 上板联调清单 🔴-1):反馈通道接线 + ADC1 引脚确认 + 两路采样同步标定.
 * -------------------------------------------------------------------------- */
static void H_App_measureFeedback(void)
{
    uint16_t bins[H_NUM_SIGNALS];
    uint8_t  i;
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        bins[i] = (uint16_t)s_res.sig[i].bin;
    }
    SigAnalysis_phasesAtBins(s_fb_buf, H_CAP_N, bins,
                             (uint8_t)H_NUM_SIGNALS, s_fb_phase);
}

void H_App_init(void)
{
    uint8_t i;

    MathUtil_init();        /* MATHACL 上电(.syscfg 未配, 手动) */
    SigAcq_init();          /* ADC0 + FIFO + DMA (采 C) */
    SigAcq_initFeedback();  /* ADC1 (采反馈 A'+B'); 反馈通道接线为上板项 */
    SigAnalysis_init();
    Reconstruct_init();     /* AD9959 (DDS_init) */
    Pll_init();             /* 两路锁相环核心 */

    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        s_target[i]     = 0;
        s_fb_phase[i]   = 0;
        s_built_freq[i] = 0u;
    }
    s_built = 0U;
}

void H_App_loop(void)
{
    uint8_t i;

    /* 1) 近同步采集: C -> s_buf(ADC0), 反馈 A'+B' -> s_fb_buf(ADC1) */
    (void)SigAcq_captureDual(s_buf, s_fb_buf, H_CAP_N);

    /* 2) 分析 C: 两路 频率/幅度/相位/波形/峰值bin */
    (void)SigAnalysis_run(s_buf, H_CAP_N, &s_res);

    if (s_built == 0U) {
        /* 3) 建链: 仅当两路结果都有效才建(否则保持等下一帧, 修 PLL-5 一次性锁死) */
        uint8_t all_ok = 1u;
        for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
            if (result_ok(&s_res.sig[i]) == 0u) { all_ok = 0u; }
        }
        if (all_ok != 0u) {
            for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
                Reconstruct_set(i, &s_res.sig[i]);        /* 频率+幅度+CPOW初值 */
                Pll_retune(i, s_res.sig[i].freq_hz);      /* 预调谐 FTW */
                s_built_freq[i] = s_res.sig[i].freq_hz;
            }
            s_built = 1U;
        }
    } else {
        /* 4) 闭环锁相: 误差 = 原信号相位 与 重建实测相位 之差 (修 PLL-1) */
        H_App_measureFeedback();
        for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
            /* 频率粗测跳变(信号源重新调频)则重设预调谐基准 */
            uint32_t f = s_res.sig[i].freq_hz;
            uint32_t df = (f > s_built_freq[i]) ? (f - s_built_freq[i])
                                                : (s_built_freq[i] - f);
            if (result_ok(&s_res.sig[i]) != 0u &&
                df >= (uint32_t)(H_FREQ_STEP_HZ / 2u)) {
                Pll_retune(i, f);
                s_built_freq[i] = f;
            }
            /* II 型 PLL 一步: 调 FTW 锁定频率与相位 */
            (void)Pll_track(i, s_res.sig[i].phase_q31, s_fb_phase[i], s_target[i]);
        }
    }

    /* 固定控制周期(SYS-4/SYS-2 修复): 等到下一周期边界, 回绕安全;
     * 若本轮处理已超过周期, (int32)(Tick-s_next_tick)>=0 立即跳出, dt 退化为处理时间. */
    while ((int32_t)(Tick - s_next_tick) < 0) { /* idle 等节拍 */ }
    s_next_tick += H_LOOP_PERIOD_MS;
}
