#include "h_app.h"
#include "h_config.h"
#include "sig_acq.h"
#include "sig_analysis.h"
#include "reconstruct.h"
#include "pll.h"
#include "mathacl_util.h"
#include "telemetry.h"
#include "tjc_hmi.h"
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
static uint32_t   s_frame = 0U;                   /* 控制周期计数(遥测/抽稀用) */

#if H_STAGE >= 3
/* 分析结果是否可用作建链: 有效标志 + 频率落在题目范围 (仅建链/闭环阶段用) */
static uint8_t result_ok(const h_sig_t *s)
{
    return (uint8_t)(s->valid != 0u &&
                     s->freq_hz >= (uint32_t)H_FREQ_MIN_HZ &&
                     s->freq_hz <= (uint32_t)H_FREQ_MAX_HZ);
}
#endif

/* ----------------------------------------------------------------------------
 *  测量重建信号 A'/B' 的相位作为锁相反馈量.
 *  A'+B' 由第 2 路 ADC(ADC1)与 C 近同步采到 s_fb_buf(SigAcq_captureDual),
 *  这里对其做 窗+FFT, 在两路已知 bin(取自对 C 的分析 s_res.sig[i].bin)处 atan2.
 *  ★上板项(见 上板联调清单 🔴-1):反馈通道接线 + ADC1 引脚确认 + 两路采样同步标定.
 * -------------------------------------------------------------------------- */
#if H_STAGE >= 4
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
#endif

#if H_TELEMETRY
/* 填一帧遥测快照: ADC 原始统计 + 分析结果 + PLL 诊断(无依赖项的阶段相应字段为 0). */
static void H_App_fillTelemetry(tel_frame_t *tf)
{
    uint8_t i;
    tf->frame = s_frame;
    tf->stage = (uint8_t)H_STAGE;
    tf->built = s_built;
    TelCore_bufStats(s_buf,    H_CAP_N, &tf->c_min,  &tf->c_mean,  &tf->c_max);
    TelCore_bufStats(s_fb_buf, H_CAP_N, &tf->fb_min, &tf->fb_mean, &tf->fb_max);
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        tf->freq[i]      = s_res.sig[i].freq_hz;
        tf->phase_q31[i] = s_res.sig[i].phase_q31;
        tf->amp[i]       = s_res.sig[i].amp;
        tf->bin[i]       = s_res.sig[i].bin;
        tf->wave[i]      = (uint8_t)s_res.sig[i].wave;
        tf->valid[i]     = s_res.sig[i].valid;
        tf->overlap[i]   = s_res.sig[i].overlap;
        tf->perr_q31[i]  = 0;  tf->ftw[i] = 0u;  tf->locked[i] = 0u;
        Pll_diag(i, &tf->perr_q31[i], &tf->ftw[i], &tf->locked[i]);
    }
}
#endif  /* H_TELEMETRY */

#if H_TJC_DISPLAY
/* 填屏幕显示内容: 两路 频率/波形/幅度 + 相位差(B′−A′ 折到 0..180°)+ 两路均锁定. */
static void H_App_fillView(tjc_view_t *v)
{
    uint8_t i;
    int32_t d, deg;
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        v->freq[i] = s_res.sig[i].freq_hz;
        v->amp[i]  = s_res.sig[i].amp;
        v->wave[i] = (uint8_t)s_res.sig[i].wave;
    }
    /* 相位差: uint32 相减再转 int32(自然回绕=最短弧), 化成度后取绝对值折到 0..180 */
    d   = (int32_t)((uint32_t)s_res.sig[1].phase_q31 - (uint32_t)s_res.sig[0].phase_q31);
    deg = TelCore_q31ToDeci(d) / 10;        /* 度, -180..180 */
    if (deg < 0) { deg = -deg; }            /* 0..180 */
    v->phase_diff_deg = (uint16_t)deg;
    v->locked = (uint8_t)((Pll_locked(0u) != 0u) && (Pll_locked(1u) != 0u));
}
#endif  /* H_TJC_DISPLAY */

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
    s_frame = 0U;

    Tel_init();      /* 串口遥测横幅(确认串口通); H_TELEMETRY=0 时为空操作 */
    Tjc_init();      /* 串口屏跳主页 + 标记首帧全发; H_TJC_DISPLAY=0 时为空操作 */
    Tjc_rxEnable();  /* 开屏 UART1 RX(触摸/相位/当前页/addt握手); RX 关时为空操作 */
}

void H_App_loop(void)
{
    /* 分阶段 bring-up(H_STAGE, 见 h_config.h): 每级是上一级的超集, 逐级排错.
     * 默认 H_STAGE=4(完整). 低阶段把后续步骤编译掉, 便于隔离问题. */
#if H_STAGE >= 4
    uint8_t i;
#endif

    /* 1) 采集(所有阶段): C -> s_buf(ADC0), 反馈 A'+B' -> s_fb_buf(ADC1) */
    (void)SigAcq_captureDual(s_buf, s_fb_buf, H_CAP_N);

#if H_STAGE >= 2
    /* 2) 分析 C: 两路 频率/幅度/相位/波形/峰值bin(频率已吸附网格, 见 sig_analysis) */
    (void)SigAnalysis_run(s_buf, H_CAP_N, &s_res);
#endif

#if H_TJC_TOUCH
    /* 屏上触摸/滑块设定的相位差目标(发挥②) -> s_target[1](B′ 相对 A′). 在 PLL 用它之前更新. */
    {
        int32_t deg;
        if (Tjc_takeTarget(&deg) != 0u) {
            if (deg < 0)   { deg = 0;   }
            if (deg > 180) { deg = 180; }
            s_target[1] = (int32_t)(((int64_t)deg * 2147483648LL) / 180);  /* 度->Q31(2^31=180°) */
        }
    }
#endif

#if H_STAGE >= 3
    if (s_built == 0U) {
        /* 3) 建链(阶段3=开环输出 / 阶段4=闭环起点): 两路结果都有效才建(修 PLL-5) */
        uint8_t k, all_ok = 1u;
        for (k = 0u; k < (uint8_t)H_NUM_SIGNALS; k++) {
            if (result_ok(&s_res.sig[k]) == 0u) { all_ok = 0u; }
        }
        if (all_ok != 0u) {
            for (k = 0u; k < (uint8_t)H_NUM_SIGNALS; k++) {
                Reconstruct_set(k, &s_res.sig[k]);        /* 频率+幅度+CPOW初值 */
                Pll_retune(k, s_res.sig[k].freq_hz);      /* 预调谐 FTW */
                s_built_freq[k] = s_res.sig[k].freq_hz;
            }
            s_built = 1U;
        }
    }
#if H_STAGE >= 4
    else {
        /* 4) 闭环锁相: 误差 = 原信号相位 与 重建实测相位 之差 (修 PLL-1) */
        H_App_measureFeedback();
        for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
            /* 频率跳变(信号源重新调频)则重设预调谐基准. 频率已吸附网格, 比较干净. */
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
#endif  /* H_STAGE >= 4 */
#endif  /* H_STAGE >= 3 */

    /* 遥测(抽稀: 每 H_TEL_DECIM 周期一行; H_TELEMETRY=0 时整体为空操作) */
#if H_TELEMETRY
    if ((s_frame % (uint32_t)H_TEL_DECIM) == 0u) {
        tel_frame_t tf;
        H_App_fillTelemetry(&tf);
        Tel_line(&tf);
    }
#endif

    /* 串口屏结果页(抽稀: 每 H_TJC_DECIM 周期; 只发变化字段; H_TJC_DISPLAY=0 时为空操作) */
#if H_TJC_DISPLAY
    if ((s_frame % (uint32_t)H_TJC_DECIM) == 0u) {
        tjc_view_t v;
        H_App_fillView(&v);
        Tjc_show(&v);
    }
#endif

    /* 串口屏波形页(抽稀): 周期 sendme 自发现当前页, 仅在波形页把 C 抽取+缩放 addt 推流.
     * 跨页 addt 无效, 故必须先确认在波形页; 推流阻塞已用就绪超时兜底(见 Tjc_pushWave). */
#if H_TJC_WAVE
    if ((s_frame % (uint32_t)H_TJC_WAVE_DECIM) == 0u) {
        Tjc_queryPage();                                  /* 屏回 0x66 更新当前页(供下次判定) */
        if (Tjc_curPage() == (uint8_t)TJC_PAGE_WAVE) {
            uint16_t wmin, wmean, wmax;
            TelCore_bufStats(s_buf, H_CAP_N, &wmin, &wmean, &wmax);
            Tjc_pushWave(s_buf, H_CAP_N, (int32_t)wmin, (int32_t)wmax);
        }
    }
#endif
    s_frame++;

    /* 固定控制周期(SYS-4/SYS-2 修复): 等到下一周期边界, 回绕安全;
     * 若本轮处理已超过周期, (int32)(Tick-s_next_tick)>=0 立即跳出, dt 退化为处理时间. */
    while ((int32_t)(Tick - s_next_tick) < 0) { /* idle 等节拍 */ }
    s_next_tick += H_LOOP_PERIOD_MS;
}
