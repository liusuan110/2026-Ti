/*
 * adc_measure.c
 * ADC10 采样模块实现
 */

#include "adc_measure.h"
#include "timer_capture.h"

#define VPP_TOPK               3U
#define VPP_MAX_WINDOWS        5U
#define VPP_MIN_POINTS_PER_WIN 16U
#define VPP_TARGET_CYCLES      4UL

/* 初始化: 将 ADC 引脚设为模拟输入 */
void ADC_init(void)
{
    /* P1.0(A0), P1.7(A7) 常驻模拟; U_o2/U_o4 复用 A0 */
    ADC10AE0 |= ADC_PIN_UO2 | ADC_PIN_UO3_FFT;
#if (ADC_PIN_UO4 != ADC_PIN_UO2)
    ADC10AE0 &= ~ADC_PIN_UO4;
#endif
}

/*
 * 单次 ADC 采样
 * 使用 AVCC (3.3V) 基准, 16 个 ADC10CLK 采样保持
 */
uint16_t ADC_singleSample(uint16_t channel)
{
    /* 关闭 ADC 后再配置 */
    ADC10CTL0 &= ~ENC;

    /* SREF_0: VR+ = AVCC, VR- = VSS
     * ADC10SHT_2: 16 x ADC10CLK 采样保持
     * ADC10ON: 开启 ADC */
    ADC10CTL0 = SREF_0 | ADC10SHT_2 | ADC10ON;

    /* 选择通道, ADC10DIV_0: 不分频, ADC10SSEL_0: ADC10OSC */
    ADC10CTL1 = channel | ADC10DIV_0 | ADC10SSEL_0;

    /* 使用 AVCC 无需等待基准建立时间 */
    /* __delay_cycles(480); */

    /* 启动转换 */
    ADC10CTL0 |= ENC | ADC10SC;

    /* 等待转换完成 */
    while (ADC10CTL1 & ADC10BUSY)
        ;

    return ADC10MEM;
}

/*
 * 流式采样测 Vpp/Vrms
 * 只保留 max/min, 不需要数组
 */
void ADC_measureVpp(uint16_t channel, uint16_t count, VppResult *result)
{
    uint16_t i, w;
    uint16_t val;
    uint16_t topk[VPP_TOPK];
    uint16_t botk[VPP_TOPK];
    uint16_t vpp_windows[VPP_MAX_WINDOWS];
    uint16_t window_count;
    uint16_t points_per_window;
    uint16_t used_windows = 0;
    uint16_t dly_loop_count = 0;
    uint32_t freq_hz_snapshot;
    int32_t diff_mv;
    uint32_t diff_code;
    uint32_t sum_top, sum_bot;

    if (result == 0) return;
    if (count < VPP_MIN_POINTS_PER_WIN) count = VPP_MIN_POINTS_PER_WIN;

    /* 将总采样点拆成多窗口，随后做中值聚合 */
    window_count = (count >= 64U) ? VPP_MAX_WINDOWS : 3U;
    points_per_window = count / window_count;
    if (points_per_window < VPP_MIN_POINTS_PER_WIN) {
        points_per_window = VPP_MIN_POINTS_PER_WIN;
    }

    /* 与频率关联：按目标覆盖 VPP_TARGET_CYCLES 个周期估算采样间隔 */
    freq_hz_snapshot = g_freq_hz;
    if (freq_hz_snapshot > 0U && points_per_window > 1U) {
        uint32_t total_cycles_per_period = SYS_FREQ / freq_hz_snapshot;
        uint32_t total_window = total_cycles_per_period * VPP_TARGET_CYCLES;
        uint32_t target_interval = total_window / (points_per_window - 1U);
        const uint32_t BASE_OVERHEAD = 100U;
        if (target_interval > BASE_OVERHEAD) {
            dly_loop_count = (uint16_t)((target_interval - BASE_OVERHEAD) / 5U);
        }
    }

    /* 配置 ADC: AVCC (3.3V) 基准, 单通道轮询采样 */
    ADC10CTL0 &= ~ENC;
    ADC10CTL0 = SREF_0 | ADC10SHT_1 | ADC10ON;
    ADC10CTL1 = channel | ADC10DIV_0 | ADC10SSEL_0;

    for (w = 0; w < window_count; w++) {
        for (i = 0; i < VPP_TOPK; i++) {
            topk[i] = 0;
            botk[i] = 1023;
        }

        for (i = 0; i < points_per_window; i++) {
            uint16_t k;
            uint16_t dly;

            ADC10CTL0 |= ENC | ADC10SC;
            while (ADC10CTL1 & ADC10BUSY)
                ;
            val = ADC10MEM;
            ADC10CTL0 &= ~ENC;

            /* Top-K */
            for (k = 0; k < VPP_TOPK; k++) {
                if (val > topk[k]) {
                    uint16_t t;
                    for (t = (uint16_t)(VPP_TOPK - 1U); t > k; t--) {
                        topk[t] = topk[t - 1U];
                    }
                    topk[k] = val;
                    break;
                }
            }

            /* Bottom-K */
            for (k = 0; k < VPP_TOPK; k++) {
                if (val < botk[k]) {
                    uint16_t t;
                    for (t = (uint16_t)(VPP_TOPK - 1U); t > k; t--) {
                        botk[t] = botk[t - 1U];
                    }
                    botk[k] = val;
                    break;
                }
            }

            if (dly_loop_count > 0U && (i + 1U < points_per_window)) {
                dly = dly_loop_count;
                while (dly--) {
                    __no_operation();
                }
            }
        }

        sum_top = 0;
        sum_bot = 0;
        for (i = 0; i < VPP_TOPK; i++) {
            sum_top += topk[i];
            sum_bot += botk[i];
        }

        if (sum_top > sum_bot) {
            diff_code = (sum_top - sum_bot) / VPP_TOPK;
        } else {
            diff_code = 0;
        }

        vpp_windows[used_windows++] = (uint16_t)(diff_code * ADC_VREF_MV / 1023U);
    }

    if (used_windows == 0U) {
        result->vpp_mv = 0;
        result->vrms_mv = 0;
        return;
    }

    /* 小数组原地排序，取中值 */
    for (i = 0; i + 1U < used_windows; i++) {
        uint16_t j;
        for (j = (uint16_t)(i + 1U); j < used_windows; j++) {
            if (vpp_windows[j] < vpp_windows[i]) {
                uint16_t tmp = vpp_windows[i];
                vpp_windows[i] = vpp_windows[j];
                vpp_windows[j] = tmp;
            }
        }
    }

    diff_mv = vpp_windows[used_windows / 2U];
    result->vpp_mv = (uint16_t)diff_mv;

    /* 对于纯正弦波: Vrms = Vpp / (2 * sqrt(2)) = Vpp * 1000 / 2828 */
    result->vrms_mv = (uint16_t)((uint32_t)diff_mv * 1000 / 2828);
}

/*
 * 连续采样 N 个点到 buffer
 * 结果为 10 位 ADC 原始值 (0~1023), 存入 int16_t 数组
 */
void ADC_sampleToBuffer(uint16_t channel, int16_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t need_restore_uo4 = 0;

    if ((channel == ADC_CH_UO4) && (ADC_PIN_UO4 != ADC_PIN_UO2)) {
        ADC10AE0 |= ADC_PIN_UO4;
        need_restore_uo4 = 1;
    }

    ADC10CTL0 &= ~ENC;
    ADC10CTL0 = SREF_0 | ADC10SHT_0 | ADC10ON;
    ADC10CTL1 = channel | ADC10DIV_0 | ADC10SSEL_0;
    /* __delay_cycles(480); */

    for (i = 0; i < len; i++) {
        ADC10CTL0 |= ENC | ADC10SC;
        while (ADC10CTL1 & ADC10BUSY)
            ;
        buf[i] = (int16_t)ADC10MEM;
        ADC10CTL0 &= ~ENC;
    }

    if (need_restore_uo4) {
        ADC10AE0 &= ~ADC_PIN_UO4;
    }
}

/*
 * ADC_sampleToBufferAdaptive() — 自适应采样窗口
 * =============================================
 * 功能:
 *   以自适应的时间间隔对指定通道连续采集 len 个 ADC 样本.
 *   根据信号频率 freq_hz 自动计算每两个采样点之间的 CPU 延时,
 *   使得 len 个采样点的总时间窗口恰好覆盖 >= 1 个信号周期.
 *
 * 参数:
 *   channel  - ADC 通道 (如 ADC_CH_UO4)
 *   buf      - 目标数组, 存放 10 位 ADC 原始值 (0~1023)
 *   len      - 需要采集的点数
 *   freq_hz  - 被测信号频率 (Hz). 传 0 则不插入延时, 以最快速度采样.
 *
 * 延时计算原理:
 *   total_window    = (SYS_FREQ / freq_hz) × WAVE_DISPLAY_CYCLES
 *   target_interval = total_window / (len - 1)
 *   dly_loop_count  = (target_interval - ADC固有开销) / 5
 *   其中 WAVE_DISPLAY_CYCLES 为屏幕目标显示周期数 (在 config.h 中配置).
 *   高频时 target_interval 小于 ADC 开销, 不加延时, 以最快速度采样.
 *
 * 注意:
 *   - 波形同步触发不在本函数内处理, 由调用方自行实现
 *     (推荐使用软件零交叉触发, 见 display.c 中的 page_wave())
 *   - 采样使用内部 2.5V 基准, ADC10SHT_0 (4 个 ADC10CLK 采样保持)
 */
void ADC_sampleToBufferAdaptive(uint16_t channel, int16_t *buf, uint16_t len, uint32_t freq_hz)
{
    uint16_t i;
    uint8_t need_restore_uo4 = 0;
    uint16_t dly_loop_count = 0;   /* 每两个采样之间的软件空循环次数 */

    if (len == 0) return;

    /* --- 如果 U_o4 使用独立引脚, 临时开启其模拟功能 --- */
    if ((channel == ADC_CH_UO4) && (ADC_PIN_UO4 != ADC_PIN_UO2)) {
        ADC10AE0 |= ADC_PIN_UO4;
        need_restore_uo4 = 1;
    }

    /* --- 根据信号频率计算采样间隔延时 --- */
    if (freq_hz > 0 && len > 1) {
        /*
         * 目标: 让 len 个采样点的总时间窗口覆盖 WAVE_DISPLAY_CYCLES 个信号周期.
         * 这样屏幕上显示的波形周期数 = WAVE_DISPLAY_CYCLES (可在 config.h 调节).
         *
         * 计算:
         *   total_window = WAVE_DISPLAY_CYCLES 个周期的 CPU 时钟数
         *   target_interval = total_window / (len - 1)
         *
         * 高频时 (如 20kHz):
         *   1 周期 = 16000000/20000 = 800 cycles
         *   2 周期的窗口 = 1600 cycles
         *   target_interval = 1600 / 95 ≈ 16 cycles
         *   16 < BASE_OVERHEAD(100), 所以 dly_loop_count = 0
         *   → ADC 以最快速度连续采样, 硬件自然限制了采样率
         *   → 实际采样窗口由 ADC 转换时间决定, 约覆盖 2~3 个周期
         *
         * 低频时 (如 1kHz):
         *   1 周期 = 16000 cycles
         *   2 周期 = 32000 cycles
         *   target_interval = 32000 / 95 ≈ 336 cycles
         *   dly_loop_count = (336 - 100) / 5 ≈ 47
         *   → 通过软件延时拉长采样间隔, 精确覆盖 2 个周期
         */
        uint32_t total_cycles_per_period = SYS_FREQ / freq_hz;
        uint32_t total_window = total_cycles_per_period * WAVE_DISPLAY_CYCLES;
        uint32_t target_interval = total_window / (len - 1);
        const uint32_t BASE_OVERHEAD = 100; /* ADC 每次转换的固有 CPU 开销 */

        if (target_interval > BASE_OVERHEAD) {
            dly_loop_count = (target_interval - BASE_OVERHEAD) / 5;
        }
        /* 否则 dly_loop_count 保持 0: ADC 以最快速度连续采, 不插入额外延时 */
    }

    /* --- 配置 ADC10 硬件 --- */
    ADC10CTL0 &= ~ENC;  /* 关闭使能, 允许修改控制寄存器 */
    ADC10CTL0 = SREF_0 | ADC10SHT_0 | ADC10ON;
    /*         SREF_0:   VR+ = AVCC (3.3V), VR- = VSS
     *         ADC10SHT_0: 4 个 ADC10CLK 采样保持 (快速采样)
     */
    ADC10CTL1 = channel | ADC10DIV_0 | ADC10SSEL_0;
    /*         ADC10DIV_0:  ADC 时钟不分频
     *         ADC10SSEL_0: ADC10OSC (~5MHz) */
    /* __delay_cycles(480); */ /* 使用 AVCC 无需等待基准稳定 */

    /* --- 连续采集 len 个点 --- */
    for (i = 0; i < len; i++) {
        ADC10CTL0 |= ENC | ADC10SC; /* 启动单次转换 */
        while (ADC10CTL1 & ADC10BUSY)
            ;                        /* 等待转换完成 */
        buf[i] = (int16_t)ADC10MEM;  /* 读取 10 位结果 (0~1023) */
        ADC10CTL0 &= ~ENC;           /* 关闭使能, 准备下次配置 */

        /* 插入计算好的延时 (最后一个点后不需要延时) */
        if (dly_loop_count > 0 && (i + 1 < len)) {
            uint16_t dly = dly_loop_count;
            while (dly--) {
                __no_operation(); /* 每次约 5 个 CPU 时钟 */
            }
        }
    }

    /* --- 恢复引脚配置 --- */
    if (need_restore_uo4) {
        ADC10AE0 &= ~ADC_PIN_UO4;
    }
}
