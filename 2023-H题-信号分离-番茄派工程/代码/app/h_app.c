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
static uint16_t   s_buf[H_CAP_N] __attribute__((aligned(4)));
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
 *  ★上板硬件项: 测量重建信号 A'/B' 的相位作为锁相反馈量.
 *  正确做法(见 上板联调清单.md): 把 A'+B' 经运放引回【ADC 第 2 路通道】,
 *  对其采集一块 -> FFT -> 在两路已知 bin 处 atan2, 得 s_fb_phase[0/1].
 *  G3507 双 12 位 ADC 正为此用.
 *
 *  当前为占位实现: 反馈相位 = 原信号相位(等价于"已完美锁定"), 使闭环成为空操作,
 *  直到第 2 路 ADC 接好. 锁相算法本身已由 host_test/test_pll.c 离板仿真验证.
 * -------------------------------------------------------------------------- */
static void H_App_measureFeedback(void)
{
    uint8_t i;
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        s_fb_phase[i] = s_res.sig[i].phase_q31;   /* TODO: 替换为第2路ADC实测 */
    }
}

void H_App_init(void)
{
    uint8_t i;

    MathUtil_init();        /* MATHACL 上电(.syscfg 未配, 手动) */
    SigAcq_init();          /* ADC0 + FIFO + DMA */
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

    /* 1) 采集混合信号 C */
    (void)SigAcq_capture(s_buf, H_CAP_N);

    /* 2) 分析: 两路 频率/幅度/相位/波形 */
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
