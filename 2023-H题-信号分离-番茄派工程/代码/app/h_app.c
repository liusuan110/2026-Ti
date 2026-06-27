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
#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
#include "pid_core.h"
#endif

#define H_CAP_N   ((uint16_t)H_FFT_N)

/* 固定控制周期(ms). SYS-4: 反馈环路控制间隔须固定(培训3 PPT 要点).
 * ★须 >= 单次 采集+FFT+反馈采集 的最长处理时间, 使实际 dt 恒等于本值(固定不抖).
 * 数字PLL 增益 PLL_*_SHIFT / 模拟PID 增益 H_PID_* 均按此 dt 整定. 默认 5ms. */
#define H_LOOP_PERIOD_MS   5u

/* 大缓冲全局, 不放栈; 4 字节对齐(ADC DMA 按 32 位字写入) */
static uint16_t   s_buf[H_CAP_N]    __attribute__((aligned(4)));  /* 混合信号 C */
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
static uint16_t   s_fb_buf[H_CAP_N] __attribute__((aligned(4)));  /* 反馈 A'+B' (第2路ADC) */
static int32_t    s_fb_phase[H_NUM_SIGNALS];      /* 重建信号 A'/B' 的实测相位(反馈) */
static int32_t    s_target[H_NUM_SIGNALS];        /* 目标相位差(发挥②, 0=同相) */
#endif
static uint32_t   s_next_tick = 0u;               /* 下一控制周期边界(SysTick ms) */
static h_result_t s_res;                          /* 对 C 的分析结果 */
static uint32_t   s_built_freq[H_NUM_SIGNALS];    /* 已建链的频率, 用于判频率跳变 */
static uint8_t    s_built = 0U;
static uint32_t   s_frame = 0U;                   /* 控制周期计数(遥测/抽稀用) */

#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
static pid_data_t s_pid[H_NUM_SIGNALS];           /* 每路 PID 状态 */
static int32_t    s_phase_deg[H_NUM_SIGNALS];     /* 发挥②相位偏移(度): 加到正交本振 CH2/CH3 */
static float      s_pd_volt[H_NUM_SIGNALS];       /* 最近鉴相电压(已去偏置, V), 遥测/锁定用 */
static uint8_t    s_pid_lockcnt[H_NUM_SIGNALS];
static uint8_t    s_pid_locked[H_NUM_SIGNALS];
static uint8_t    s_pd_fail = 0u;     /* 鉴相 ADC 读超时标志(遥测可见) */
#endif

#if H_STAGE >= 3
/* 分析结果是否可用作建链: 有效标志 + 频率落在题目范围 (仅建链/闭环阶段用) */
static uint8_t result_ok(const h_sig_t *s)
{
    return (uint8_t)(s->valid != 0u &&
                     s->freq_hz >= (uint32_t)H_FREQ_MIN_HZ &&
                     s->freq_hz <= (uint32_t)H_FREQ_MAX_HZ);
}
#endif

#if (H_STAGE >= 4) && (H_LOCK_METHOD == H_LOCK_DIGITAL_PLL)
/* ----------------------------------------------------------------------------
 *  数字PLL: 测量重建信号 A'/B' 的相位作为锁相反馈量.
 *  A'+B' 由第 2 路 ADC(ADC1)与 C 同步采到 s_fb_buf(SigAcq_captureDual),
 *  这里对其做 窗+FFT, 在两路已知 bin(取自对 C 的分析 s_res.sig[i].bin)处 atan2.
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
#endif

#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
/* 模拟鉴相: 按当前基频 + PID 输出 + 发挥②偏移, 驱动 4 通道(CH0/1输出 + CH2/3正交本振). */
static void H_App_driveAnalog(void)
{
    uint8_t i;
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        float    freq = (float)s_built_freq[i] + s_pid[i].out;        /* PID 微调频率 */
        int32_t  deg  = s_phase_deg[i];
        uint16_t refp;
        if (deg < 0)   { deg = 0;   }
        if (deg > 180) { deg = 180; }
        /* 正交本振相位 = 90°(H_QUAD_PHASE14) + 发挥②偏移(度->14位相位字, 16384=360°) */
        refp = (uint16_t)(((uint32_t)H_QUAD_PHASE14 +
                           (uint32_t)(((int64_t)deg * 16384) / 360)) & 0x3FFFu);
        Reconstruct_setAnalog(i, freq, refp);
    }
    Reconstruct_update();   /* 4 通道一起生效 */
}

/* 模拟鉴相: 一步闭环 —— 读两路鉴相电压 -> 每路 PID(增益按频率调度) -> 微调频率 -> 驱动 4 通道. */
static void H_App_trackAnalog(void)
{
    uint16_t pd[H_NUM_SIGNALS];
    uint8_t  i;

    /* 频率跳变(信号源重调)则重设基频 + 复位 PID. 频率已吸附网格, 比较干净. */
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        uint32_t f  = s_res.sig[i].freq_hz;
        uint32_t df = (f > s_built_freq[i]) ? (f - s_built_freq[i]) : (s_built_freq[i] - f);
        if ((result_ok(&s_res.sig[i]) != 0u) && (df >= (uint32_t)(H_FREQ_STEP_HZ / 2u))) {
            s_built_freq[i] = f;
            PidCore_init(&s_pid[i], 0.0f);
        }
    }

    if (SigAcq_readPD(pd) == 0u) {    /* 鉴相 ADC 读超时: 不喂幻象误差跑飞 */
        s_pd_fail = 1u;
        for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) { s_pid_lockcnt[i] = 0u; s_pid_locked[i] = 0u; }
        return;   /* 保持上一拍 DDS 驱动(维持上次 FTW), 跳过本拍 PID + 驱动 */
    }
    s_pd_fail = 0u;
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        pid_param_t pp;
        float volt = ((float)pd[i] * 3.3f / 4096.0f) - H_PD_OFFSET_V;   /* 去偏置 -> 相位误差 */
        float gfac = (float)s_built_freq[i] * H_PID_GAIN_PER_HZ;        /* 增益按频率调度(归一50k) */
        s_pd_volt[i] = volt;
        pp.kp = H_PID_KP_50K * gfac;
        pp.ki = H_PID_KI_50K * gfac;
        pp.kd = H_PID_KD_50K * gfac;
        pp.int_max = H_PID_INT_MAX;
        pp.out_max = H_PID_OUT_MAX;
        s_pid[i].feedback = volt;   /* 目标 0(正交锁定) */
        (void)PidCore_update(&pp, &s_pid[i], (float)H_LOOP_PERIOD_MS * 1e-3f);
        /* 锁定判据: |鉴相电压| 连续多帧小于阈值 */
        if ((volt < H_PID_LOCK_VOLT) && (volt > -H_PID_LOCK_VOLT)) {
            if (s_pid_lockcnt[i] < 255u) { s_pid_lockcnt[i]++; }
        } else {
            s_pid_lockcnt[i] = 0u;
        }
        s_pid_locked[i] = (s_pid_lockcnt[i] >= (uint8_t)H_PID_LOCK_FRAMES) ? 1u : 0u;
    }
    H_App_driveAnalog();
}

/* 两路是否均锁定(供遥测/屏) */
static uint8_t H_App_analogLocked(void)
{
    return (uint8_t)((s_pid_locked[0] != 0u) && (s_pid_locked[1] != 0u));
}
#endif  /* H_LOCK_ANALOG_PID */

#if H_TELEMETRY
/* 填一帧遥测快照: ADC 原始统计 + 分析结果 + 锁相诊断(按锁相方式取 PLL 或 PID 状态). */
static void H_App_fillTelemetry(tel_frame_t *tf)
{
    uint8_t i;
    tf->frame = s_frame;
    tf->stage = (uint8_t)H_STAGE;
    tf->built = s_built;
    TelCore_bufStats(s_buf, H_CAP_N, &tf->c_min, &tf->c_mean, &tf->c_max);
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
    TelCore_bufStats(s_fb_buf, H_CAP_N, &tf->fb_min, &tf->fb_mean, &tf->fb_max);
#else
    tf->fb_min = 0u; tf->fb_mean = 0u; tf->fb_max = 0u;   /* 模拟模式无反馈块 */
#endif
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        tf->freq[i]      = s_res.sig[i].freq_hz;
        tf->phase_q31[i] = s_res.sig[i].phase_q31;
        tf->amp[i]       = s_res.sig[i].amp;
        tf->bin[i]       = s_res.sig[i].bin;
        tf->wave[i]      = (uint8_t)s_res.sig[i].wave;
        tf->valid[i]     = s_res.sig[i].valid;
        tf->overlap[i]   = s_res.sig[i].overlap;
        tf->perr_q31[i]  = 0;  tf->ftw[i] = 0u;  tf->locked[i] = 0u;
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
        Pll_diag(i, &tf->perr_q31[i], &tf->ftw[i], &tf->locked[i]);
#else
        /* 模拟模式: ftw 字段借显鉴相电压(mV), locked=PID锁定 */
        tf->ftw[i]    = (uint32_t)((s_pd_volt[i] + H_PD_OFFSET_V) * 1000.0f);
        tf->perr_q31[i] = (int32_t)s_pd_fail;   /* 模拟: perr 字段借显鉴相读超时标志 */
        tf->locked[i] = s_pid_locked[i];
#endif
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
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
    v->locked = (uint8_t)((Pll_locked(0u) != 0u) && (Pll_locked(1u) != 0u));
#else
    v->locked = H_App_analogLocked();
#endif
}
#endif  /* H_TJC_DISPLAY */

void H_App_init(void)
{
    uint8_t i;

    MathUtil_init();        /* MATHACL 上电(.syscfg 未配, 手动) */
    SigAcq_init();          /* ADC0 + FIFO + DMA (采 C) */
    SigAnalysis_init();
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
    SigAcq_initFeedback();  /* ADC1 块采反馈 A'+B'; 反馈通道接线为上板项 */
    Reconstruct_init();     /* AD9959 2 通道 */
    Pll_init();             /* 两路数字锁相环核心 */
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        s_target[i]   = 0;
        s_fb_phase[i] = 0;
    }
#else  /* H_LOCK_ANALOG_PID */
    SigAcq_initPD();        /* ADC1 改 2 通道单次读鉴相电压 */
    Reconstruct_initAnalog();  /* AD9959 4 通道(输出 + 正交本振) */
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
        PidCore_init(&s_pid[i], 0.0f);
        s_phase_deg[i]   = 0;
        s_pd_volt[i]     = 0.0f;
        s_pid_lockcnt[i] = 0u;
        s_pid_locked[i]  = 0u;
    }
#endif
    for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) { s_built_freq[i] = 0u; }
    s_built = 0U;
    s_frame = 0U;

    Tel_init();      /* 串口遥测横幅(确认串口通); H_TELEMETRY=0 时为空操作 */
    Tjc_init();      /* 串口屏跳主页 + 标记首帧全发; H_TJC_DISPLAY=0 时为空操作 */
    Tjc_rxEnable();  /* 开屏 UART1 RX(触摸/相位/当前页/addt握手); RX 关时为空操作 */
}

void H_App_loop(void)
{
    /* 1) 采集(所有阶段): 数字PLL 双路同步采 C+反馈; 模拟PID 只采 C(鉴相走 ADC1 单次读) */
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
    (void)SigAcq_captureDual(s_buf, s_fb_buf, H_CAP_N);
#else
    (void)SigAcq_capture(s_buf, H_CAP_N);
#endif

#if H_STAGE >= 2
    /* 2) 分析 C: 两路 频率/幅度/相位/波形/峰值bin(频率已吸附网格) */
    (void)SigAnalysis_run(s_buf, H_CAP_N, &s_res);
#endif

#if H_TJC_TOUCH
    /* 屏上触摸/滑块设定的相位差目标(发挥②) */
    {
        int32_t deg;
        if (Tjc_takeTarget(&deg) != 0u) {
            if (deg < 0)   { deg = 0;   }
            if (deg > 180) { deg = 180; }
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
            s_target[1] = (int32_t)(((int64_t)deg * 2147483648LL) / 180);  /* 度->Q31 给PLL */
#else
            s_phase_deg[1] = deg;   /* 度: 给 B 路正交本振 CH3 的发挥②偏移 */
#endif
        }
    }
#endif

#if H_STAGE >= 3
    if (s_built == 0U) {
        /* 3) 建链(阶段3=开环输出 / 阶段4=闭环起点): 两路结果都有效才建 */
        uint8_t k, all_ok = 1u;
        for (k = 0u; k < (uint8_t)H_NUM_SIGNALS; k++) {
            if (result_ok(&s_res.sig[k]) == 0u) { all_ok = 0u; }
        }
        if (all_ok != 0u) {
            for (k = 0u; k < (uint8_t)H_NUM_SIGNALS; k++) {
                s_built_freq[k] = s_res.sig[k].freq_hz;
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
                Reconstruct_set(k, &s_res.sig[k]);        /* 频率+幅度+CPOW初值 */
                Pll_retune(k, s_res.sig[k].freq_hz);      /* 预调谐 FTW */
#else
                PidCore_init(&s_pid[k], 0.0f);
                s_pid_lockcnt[k] = 0u; s_pid_locked[k] = 0u;
#endif
            }
#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
            H_App_driveAnalog();   /* 4 通道按基频起振 */
#endif
            s_built = 1U;
        }
    }
#if H_STAGE >= 4
    else {
        /* 4) 闭环锁相 */
#if H_LOCK_METHOD == H_LOCK_DIGITAL_PLL
        uint8_t i;
        H_App_measureFeedback();
        for (i = 0u; i < (uint8_t)H_NUM_SIGNALS; i++) {
            uint32_t f  = s_res.sig[i].freq_hz;
            uint32_t df = (f > s_built_freq[i]) ? (f - s_built_freq[i]) : (s_built_freq[i] - f);
            if (result_ok(&s_res.sig[i]) != 0u && df >= (uint32_t)(H_FREQ_STEP_HZ / 2u)) {
                Pll_retune(i, f);
                s_built_freq[i] = f;
            }
            (void)Pll_track(i, s_res.sig[i].phase_q31, s_fb_phase[i], s_target[i]);
        }
#else  /* 模拟鉴相 + PID */
        H_App_trackAnalog();
#endif
    }
#endif  /* H_STAGE >= 4 */
#endif  /* H_STAGE >= 3 */

    /* 遥测(抽稀) */
#if H_TELEMETRY
    if ((s_frame % (uint32_t)H_TEL_DECIM) == 0u) {
        tel_frame_t tf;
        H_App_fillTelemetry(&tf);
        Tel_line(&tf);
    }
#endif

    /* 串口屏结果页(抽稀) */
#if H_TJC_DISPLAY
    if ((s_frame % (uint32_t)H_TJC_DECIM) == 0u) {
        tjc_view_t v;
        H_App_fillView(&v);
        Tjc_show(&v);
    }
#endif

    /* 串口屏波形页(抽稀): 仅在波形页把 C 抽取+缩放 addt 推流 */
#if H_TJC_WAVE
    if ((s_frame % (uint32_t)H_TJC_WAVE_DECIM) == 0u) {
        Tjc_queryPage();
        if (Tjc_curPage() == (uint8_t)TJC_PAGE_WAVE) {
            uint16_t wmin, wmean, wmax;
            TelCore_bufStats(s_buf, H_CAP_N, &wmin, &wmean, &wmax);
            Tjc_pushWave(s_buf, H_CAP_N, (int32_t)wmin, (int32_t)wmax);
        }
    }
#endif
    s_frame++;

    /* 固定控制周期(回绕安全): 等到下一周期边界; 处理超时则立即继续(dt 退化为处理时间). */
    while ((int32_t)(Tick - s_next_tick) < 0) { /* idle 等节拍 */ }
    s_next_tick += H_LOOP_PERIOD_MS;
}
