/*
 * adc_measure.c
 * ADC10 采样模块实现
 */

#include "adc_measure.h"
#include "timer_capture.h"

#define VPP_TOPK               10U
#define VPP_DISCARD            4U
#define VPP_MAX_WINDOWS        5U
#define VPP_MIN_POINTS_PER_WIN 16U
#define VPP_TARGET_CYCLES      4UL
#define ADC_BASE_OVERHEAD_CYC  100UL
#define ADC_DLY_LOOP_CYC       5UL

static uint16_t vpp_calcDelayLoops(uint32_t freq_hz, uint16_t points_per_window)
{
    uint32_t total_cycles_per_period;
    uint32_t total_window;
    uint32_t target_interval;

    if (freq_hz == 0U || points_per_window <= 1U) return 0U;

    total_cycles_per_period = SYS_FREQ / freq_hz;
    total_window = total_cycles_per_period * VPP_TARGET_CYCLES;
    target_interval = total_window / (uint32_t)(points_per_window - 1U);

    if (target_interval <= ADC_BASE_OVERHEAD_CYC) return 0U;

    return (uint16_t)((target_interval - ADC_BASE_OVERHEAD_CYC) / ADC_DLY_LOOP_CYC);
}

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
 * 流式采样测 Vpp/Vrms
 * 只保留 max/min, 不需要数组
 */
void ADC_measureVpp(uint16_t channel, uint16_t count, VppResult *result)
{
    uint16_t i, w;
    uint16_t val;
    uint16_t vpp_windows[VPP_MAX_WINDOWS];
    uint16_t window_count;
    uint16_t points_per_window;
    uint16_t used_windows = 0;
    uint16_t dly_loop_count = 0;
    uint32_t freq_hz_snapshot;
    uint32_t freq_period_snapshot;
    int32_t diff_mv;
    uint32_t diff_code;

    if (result == 0) return;
    if (count < VPP_MIN_POINTS_PER_WIN) count = VPP_MIN_POINTS_PER_WIN;

    /* 将总采样点拆成多窗口，随后做中值聚合 */
    window_count = (count >= 64U) ? VPP_MAX_WINDOWS : 3U;
    points_per_window = count / window_count;
    if (points_per_window < VPP_MIN_POINTS_PER_WIN) {
        points_per_window = VPP_MIN_POINTS_PER_WIN;
    }

    /* 优先使用周期寄存器快照, 避免频率整数化带来的抖动 */
    freq_period_snapshot = g_freq_period;
    if (freq_period_snapshot > 0U) {
        freq_hz_snapshot = SYS_FREQ / freq_period_snapshot;
    } else {
        freq_hz_snapshot = g_freq_hz;
    }
    dly_loop_count = vpp_calcDelayLoops(freq_hz_snapshot, points_per_window);

    /* 配置 ADC: AVCC (3.3V) 基准, 单通道轮询采样 */
    ADC10CTL0 &= ~ENC;
    ADC10CTL0 = SREF_0 | ADC10SHT_1 | ADC10ON;
    ADC10CTL1 = channel | ADC10DIV_0 | ADC10SSEL_0;

    for (w = 0; w < window_count; w++) {
        /* 使用基于 32 个直方图 Bin 的统计法过滤方波尖刺 */
        uint8_t hist[32];
        uint16_t rem_sum[32];
        for (i = 0; i < 32; i++) {
            hist[i] = 0;
            rem_sum[i] = 0;
        }

        for (i = 0; i < points_per_window; i++) {
            uint16_t dly;
            uint8_t bin;

            ADC10CTL0 |= ENC | ADC10SC;
            while (ADC10CTL1 & ADC10BUSY)
                ;
            val = ADC10MEM;
            ADC10CTL0 &= ~ENC;

            bin = (uint8_t)(val >> 5); /* 分成32段 */
            hist[bin]++;
            rem_sum[bin] += (val & 0x1F); /* 记录段内尾数求精准平均 */

            if (dly_loop_count > 0U && (i + 1U < points_per_window)) {
                dly = dly_loop_count;
                while (dly--) {
                    __no_operation();
                }
            }
        }

        /* 寻找众数以过滤过冲：从两端寻找点数大于阈值的直方图区 */
        {
            uint8_t threshold = (uint8_t)(points_per_window / 12U); 
            int8_t top_bin = 31;
            int8_t bot_bin = 0;

            if (threshold < 2) threshold = 2;

            while (top_bin >= 0 && hist[top_bin] < threshold) {
                top_bin--;
            }

            while (bot_bin <= 31 && hist[bot_bin] < threshold) {
                bot_bin++;
            }

            if (top_bin >= bot_bin && top_bin >= 0 && bot_bin <= 31) {
                uint16_t top_val = (top_bin << 5) + (rem_sum[top_bin] / hist[top_bin]);
                uint16_t bot_val = (bot_bin << 5) + (rem_sum[bot_bin] / hist[bot_bin]);
                diff_code = top_val - bot_val;
                /* 由于方波顶端有小斜率，众数段可能不在最顶点，补偿3% */
                diff_code = (diff_code * 103U) / 100U;
            } else {
                diff_code = 0;
            }
        }

        vpp_windows[used_windows++] = (uint16_t)(diff_code * ADC_VREF_MV / 1023U);

        /* 每个窗口结束后重新读取捕获频率, 并做轻量低通减少抖动 */
        freq_period_snapshot = g_freq_period;
        if (freq_period_snapshot > 0U) {
            uint32_t fresh_hz = SYS_FREQ / freq_period_snapshot;
            if (freq_hz_snapshot == 0U) {
                freq_hz_snapshot = fresh_hz;
            } else {
                freq_hz_snapshot = (freq_hz_snapshot * 3U + fresh_hz) / 4U;
            }
            dly_loop_count = vpp_calcDelayLoops(freq_hz_snapshot, points_per_window);
        }
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
    
    /* 硬件存在 ~250mV 偏差，加入补偿值。避免零输入时空跳变 */
    if (diff_mv > 100) {
        diff_mv += 250;
    }

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
uint32_t ADC_sampleToBufferAdaptive(uint16_t channel, int16_t *buf, uint16_t len, uint32_t freq_hz)
{
    uint16_t i;
    uint8_t need_restore_uo4 = 0;
    uint16_t dly_loop_count = 0;   /* 每两个采样之间的软件空循环次数 */
    uint32_t actual_interval = 90; 
    uint16_t t_start, t_end;

    if (len == 0) return actual_interval;

    /* --- 如果 U_o4 使用独立引脚, 临时开启其模拟功能 --- */
    if ((channel == ADC_CH_UO4) && (ADC_PIN_UO4 != ADC_PIN_UO2)) {
        ADC10AE0 |= ADC_PIN_UO4;
        need_restore_uo4 = 1;
    }

    /* --- 根据信号频率计算采样间隔延时 --- */
    if (freq_hz > 0 && len > 1) {
        uint32_t total_cycles_per_period = SYS_FREQ / freq_hz;
        uint32_t total_window = total_cycles_per_period * WAVE_DISPLAY_CYCLES;
        uint32_t target_interval = total_window / (len - 1);
        const uint32_t BASE_OVERHEAD = 90; /* ADC 每次转换的固有 CPU 开销修正为 90 */

        if (target_interval > BASE_OVERHEAD) {
            dly_loop_count = (target_interval - BASE_OVERHEAD) / 5;
        }
    }

    /* --- 配置 ADC10 硬件 --- */
    ADC10CTL0 &= ~ENC;  /* 关闭使能, 允许修改控制寄存器 */
    ADC10CTL0 = SREF_0 | ADC10SHT_0 | ADC10ON;
    ADC10CTL1 = channel | ADC10DIV_0 | ADC10SSEL_0;

    /* --- 连续采集 len 个点 --- */
    t_start = TA0R; /* 记录定时器起始时刻，这需要保证TA0是连续模式运行的(如本工程中的实现) */
    
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
    
    t_end = TA0R;
    
    if (dly_loop_count == 0) {
        /* 高频时因为没进延时函数，难以直接预估循环时间，靠截取硬件连续计数的Timer直接算出实际采样平均时间 */
        uint16_t diff = t_end - t_start;
        actual_interval = (uint32_t)diff / len;
    } else {
        /* 低频时由于主导的软件延时特别长，不用依靠时间戳来推算，而直接根据精确的已知延时时间决定，防16位定时器溢出反绕 */
        actual_interval = 90 + dly_loop_count * 5;
    }

    /* --- 恢复引脚配置 --- */
    if (need_restore_uo4) {
        ADC10AE0 &= ~ADC_PIN_UO4;
    }
    
    return actual_interval;
}
