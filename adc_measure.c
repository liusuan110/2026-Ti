/*
 * adc_measure.c
 * ADC10 采样模块实现
 */

#include "adc_measure.h"

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
    uint16_t i;
    uint16_t val;
    uint16_t max_val = 0;
    uint16_t min_val = 1023;
    int32_t diff_mv;

    /* 配置 ADC: AVCC (3.3V) 基准, 连续单通道 */
    ADC10CTL0 &= ~ENC;
    ADC10CTL0 = SREF_0 | ADC10SHT_1 | ADC10ON;
    ADC10CTL1 = channel | ADC10DIV_0 | ADC10SSEL_0;
    /* __delay_cycles(480); */

    for (i = 0; i < count; i++) {
        ADC10CTL0 |= ENC | ADC10SC;
        while (ADC10CTL1 & ADC10BUSY)
            ;
        val = ADC10MEM;
        ADC10CTL0 &= ~ENC;

        if (val > max_val) max_val = val;
        if (val < min_val) min_val = val;
    }

    /* Vpp = (max - min) * 3300 / 1023  (mV) */
    diff_mv = (int32_t)(max_val - min_val) * ADC_VREF_MV / 1023;
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
