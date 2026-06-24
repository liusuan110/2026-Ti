#include "sig_acq.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include "Tick.h"   /* 采集超时用 SysTick ms 计数 Tick(建议7 防卡死) */

/* 单块采集硬超时(ms): 正常一块 1024 点 @~1MSa/s ≈ 1ms, 取 50ms 远超之.
 * 仅用于"DMA_DONE 永不来"时不死等(SysTick 每 1ms 唤醒 __WFE, 故能跳出). */
#define H_ACQ_TIMEOUT_MS    50u

/* ============================================================================
 *  ADC12 + FIFO + DMA 块采集 (番茄派 MSPM0G3507, ADC0)
 *  - 全部用 DriverLib 直接配置(不依赖 SysConfig 里的 ADC/DMA 模块),
 *    照搬 SDK 例程 adc12_max_freq_dma 的已验证流程.
 *  - FIFO: 2 个 12-bit 样本压进 1 个 32-bit 字 => DMA 传输 N/2 个字(WIDTH_WORD),
 *    目的 buffer 仍是 uint16_t[N], DMA 写入后即 N 个 12-bit 样本.
 *  - 采样率: 软件触发 + repeat, fs ≈ ADCCLK/(采样窗+转换周期). 用 SYSOSC(32MHz)
 *    + 采样时间 17 周期 => fs 约 1.0~1.03 MSa/s (>1MSa/s, 可看 100kHz 三角的5次谐波).
 *    ⚠️ 实际 fs 必须上板标定(给已知频率, 看落在哪个 FFT bin)后写回 h_config 的 H_ADC_FS_HZ.
 *  - TODO(精确锁率): 改用 TIMG(BUSCLK, 周期模式) 事件触发 ADC(trigSrc=EVENT,
 *    pub/sub 同通道号), 可得严格 fs. 侦察已备 API, 见仓库 README.
 * ========================================================================== */

/* 番茄派 ADC 输入: 通道2 = PA25 (IOMUX PINCM55). ⚠️ 按你板子接线确认 */
#define H_ADC_INST          ADC0
#define H_ADC_INT_IRQN      ADC0_INT_IRQn
#define H_ADC_CHAN          DL_ADC12_INPUT_CHAN_2
#define H_ADC_MEM           DL_ADC12_MEM_IDX_0
#define H_ADC_PIN_IOMUX     IOMUX_PINCM55     /* PA25 */
#define H_DMA_CHAN          0U                /* DMA 通道 0 */
#define H_ADC_SAMPLE_TIME   17U               /* 采样窗周期数(标定 fs 用) */

/* ---- 第2路反馈 ADC (ADC1): 采 重建信号 A'+B' 供 PLL 鉴相 ----
 * ★引脚/通道按番茄派反馈接线确认(占位 TODO); G3507 ADC0/ADC1 双实例支持同时采.
 * ★同步: SigAcq_captureDual 用"两路软件启动相隔数条指令"近似同步(残留小且基本恒定,
 *   可上板标定补偿); 若需零时间差, 改用同一 TIMG 周期事件经事件总线同时触发(见 captureDual 注释). */
#define H_FB_INST           ADC1
#define H_FB_INT_IRQN       ADC1_INT_IRQn
#define H_FB_CHAN           DL_ADC12_INPUT_CHAN_0   /* TODO: 确认 ADC1 通道 */
#define H_FB_MEM            DL_ADC12_MEM_IDX_0
#define H_FB_PIN_IOMUX      IOMUX_PINCM1            /* TODO: 确认 ADC1 反馈引脚 */
#define H_FB_DMA_CHAN       1U                      /* DMA 通道 1 */

/* ---- 模拟鉴相模式(H_LOCK_METHOD==H_LOCK_ANALOG_PID): ADC1 改 2 通道单次读鉴相直流电压 ----
 * 配方照学长生成配置: initSeqSample(单次, 软件触发, MEM0->MEM1) + MEM1 中断.
 * ★两路鉴相电压输入引脚/通道按你硬件确认(占位 TODO). */
#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
#define H_PD_CHAN_A         DL_ADC12_INPUT_CHAN_1   /* TODO: ADC1 鉴相A 通道 */
#define H_PD_CHAN_B         DL_ADC12_INPUT_CHAN_2   /* TODO: ADC1 鉴相B 通道 */
#define H_PD_PIN_A_IOMUX    IOMUX_PINCM2            /* TODO: 鉴相A 引脚 */
#define H_PD_PIN_B_IOMUX    IOMUX_PINCM3            /* TODO: 鉴相B 引脚 */
#endif

/* ---- P1: 触发源(由 h_config.h 的 H_ACQ_USE_TIMER_TRIGGER 选择) ----
 * 定时器模式: ADC 触发源=事件, 每个事件触发下一次转换; ADC0/ADC1 订阅同一事件通道
 *   => 同一时刻采样(零时间差). 软件模式: 软件启动 + 自动连转(原路径, 作回退). */
#if H_ACQ_USE_TIMER_TRIGGER
#define H_ADC_TRIGSRC       DL_ADC12_TRIG_SRC_EVENT
#define H_ADC_MEMTRIG       DL_ADC12_TRIGGER_MODE_TRIGGER_NEXT
#define H_ADC_TRIG_EVENT_CHAN  1u   /* ★必须 == syscfg TIMER_ADC.event1PublisherChannel */
#if (H_ADC_TRIG_EVENT_CHAN != TIMER_ADC_INST_PUB_0_CH)
#error "H_ADC_TRIG_EVENT_CHAN 与 syscfg TIMER_ADC 发布通道不一致 (改了 syscfg 须同步此值)"
#endif
#else
#define H_ADC_TRIGSRC       DL_ADC12_TRIG_SRC_SOFTWARE
#define H_ADC_MEMTRIG       DL_ADC12_TRIGGER_MODE_AUTO_NEXT
#endif

static volatile uint8_t s_dma_done    = 0U;
static volatile uint8_t s_fb_dma_done = 0U;
#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
static volatile uint8_t  s_pd_done    = 0U;   /* 鉴相 2 通道转换完成标志 */
static volatile uint16_t s_pd_raw[2]  = {0u, 0u};  /* 两路鉴相 ADC 原始码 */
#endif

#if H_ACQ_USE_TIMER_TRIGGER
/* 采样定时器启停: 用于"停表->两路都使能->启表", 保证第一个事件同时命中两路(块对齐+零时间差). */
static inline void acq_timer_stop(void)  { DL_TimerA_stopCounter(TIMER_ADC_INST);  }
static inline void acq_timer_start(void) { DL_TimerA_startCounter(TIMER_ADC_INST); }
#endif

static const DL_ADC12_ClockConfig kAdcClk = {
    .clockSel    = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_1,
    .freqRange   = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};

/* DMA: FIFO(源固定) -> 内存(目的递增), 32位字, 由 ADC 的 FIFO 阈值硬件触发 */
static const DL_DMA_Config kDmaCfg = {
    .transferMode  = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode  = DL_DMA_NORMAL_MODE,
    .destIncrement = DL_DMA_ADDR_INCREMENT,
    .srcIncrement  = DL_DMA_ADDR_UNCHANGED,
    .destWidth     = DL_DMA_WIDTH_WORD,
    .srcWidth      = DL_DMA_WIDTH_WORD,
    .trigger       = DMA_ADC0_EVT_GEN_BD_TRIG,
    .triggerType   = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

/* 反馈路 DMA: 与上同, 触发源换成 ADC1 的 FIFO 事件 */
static const DL_DMA_Config kDmaCfgFb = {
    .transferMode  = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode  = DL_DMA_NORMAL_MODE,
    .destIncrement = DL_DMA_ADDR_INCREMENT,
    .srcIncrement  = DL_DMA_ADDR_UNCHANGED,
    .destWidth     = DL_DMA_WIDTH_WORD,
    .srcWidth      = DL_DMA_WIDTH_WORD,
    .trigger       = DMA_ADC1_EVT_GEN_BD_TRIG,
    .triggerType   = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

/* 一路 ADC 的"采前冲洗": 停转换 + toggle FIFO 清残留 + 清 DMA_DONE 标志 (ACQ-4) */
static void adc_flush(ADC12_Regs *inst)
{
    DL_ADC12_stopConversion(inst);
    DL_ADC12_disableConversions(inst);
    DL_ADC12_disableFIFO(inst);
    DL_ADC12_enableFIFO(inst);
    DL_ADC12_clearInterruptStatus(inst, DL_ADC12_INTERRUPT_DMA_DONE);
}

/* 配置一路 ADC(实例/通道/引脚/DMA通道)为 单通道+repeat+软件触发+FIFO+DMA */
static void adc_setup(ADC12_Regs *inst, uint32_t chan, uint32_t mem,
                      uint32_t pin_iomux, uint8_t dma_chan,
                      const DL_DMA_Config *dma_cfg, IRQn_Type irqn)
{
    DL_ADC12_reset(inst);
    DL_ADC12_enablePower(inst);
    delay_cycles(16);
    DL_GPIO_initPeripheralAnalogFunction(pin_iomux);
    DL_ADC12_setClockConfig(inst, (DL_ADC12_ClockConfig *)&kAdcClk);
    DL_ADC12_initSingleSample(inst,
        DL_ADC12_REPEAT_MODE_ENABLED, DL_ADC12_SAMPLING_SOURCE_AUTO,
        H_ADC_TRIGSRC, DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(inst, mem, chan, DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, H_ADC_MEMTRIG,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
#if H_ACQ_USE_TIMER_TRIGGER
    /* 订阅采样定时器事件(两路同一通道 => 同一时刻触发) */
    DL_ADC12_setSubscriberChanID(inst, (uint8_t)H_ADC_TRIG_EVENT_CHAN);
#endif
    DL_ADC12_enableFIFO(inst);
    DL_ADC12_setPowerDownMode(inst, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(inst, H_ADC_SAMPLE_TIME);
    DL_ADC12_enableDMA(inst);
    DL_ADC12_setDMASamplesCnt(inst, 6);
    DL_ADC12_enableDMATrigger(inst, DL_ADC12_DMA_MEM10_RESULT_LOADED);
    DL_ADC12_clearInterruptStatus(inst, DL_ADC12_INTERRUPT_DMA_DONE);
    DL_ADC12_enableInterrupt(inst, DL_ADC12_INTERRUPT_DMA_DONE);
    DL_DMA_initChannel(DMA, dma_chan, (DL_DMA_Config *)dma_cfg);
    DL_ADC12_enableConversions(inst);
    NVIC_EnableIRQ(irqn);
}

void SigAcq_init(void)
{
    /* 第1路: 采混合信号 C (ADC0 / 通道2 / PA25) */
    adc_setup(H_ADC_INST, H_ADC_CHAN, H_ADC_MEM, H_ADC_PIN_IOMUX,
              H_DMA_CHAN, &kDmaCfg, H_ADC_INT_IRQN);
}

void SigAcq_initFeedback(void)
{
    /* 第2路: 采重建信号 A'+B' 作 PLL 反馈 (ADC1) */
    adc_setup(H_FB_INST, H_FB_CHAN, H_FB_MEM, H_FB_PIN_IOMUX,
              H_FB_DMA_CHAN, &kDmaCfgFb, H_FB_INT_IRQN);
}

#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
/* 模拟鉴相: ADC1 配 2 通道单次序列(MEM0=鉴相A, MEM1=鉴相B), 软件触发, MEM1 完成中断. */
void SigAcq_initPD(void)
{
    DL_ADC12_reset(H_FB_INST);
    DL_ADC12_enablePower(H_FB_INST);
    delay_cycles(16);
    DL_GPIO_initPeripheralAnalogFunction(H_PD_PIN_A_IOMUX);
    DL_GPIO_initPeripheralAnalogFunction(H_PD_PIN_B_IOMUX);
    DL_ADC12_setClockConfig(H_FB_INST, (DL_ADC12_ClockConfig *)&kAdcClk);
    DL_ADC12_initSeqSample(H_FB_INST,
        DL_ADC12_REPEAT_MODE_DISABLED, DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE, DL_ADC12_SEQ_START_ADDR_00, DL_ADC12_SEQ_END_ADDR_01,
        DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(H_FB_INST, DL_ADC12_MEM_IDX_0, H_PD_CHAN_A,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(H_FB_INST, DL_ADC12_MEM_IDX_1, H_PD_CHAN_B,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED, DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setPowerDownMode(H_FB_INST, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(H_FB_INST, H_ADC_SAMPLE_TIME);
    DL_ADC12_clearInterruptStatus(H_FB_INST, DL_ADC12_INTERRUPT_MEM1_RESULT_LOADED);
    DL_ADC12_enableInterrupt(H_FB_INST, DL_ADC12_INTERRUPT_MEM1_RESULT_LOADED);
    DL_ADC12_enableConversions(H_FB_INST);
    NVIC_EnableIRQ(H_FB_INT_IRQN);
}

/* 软件触发 ADC1 单次序列, 读两路鉴相 12 位码到 out[0]/out[1]. 成功返回1, 超时返回0. */
uint8_t SigAcq_readPD(uint16_t *out)
{
    uint32_t t0 = Tick;
    s_pd_done = 0U;
    DL_ADC12_startConversion(H_FB_INST);
    while (s_pd_done == 0U) {
        __WFE();
        if ((uint32_t)(Tick - t0) > (uint32_t)H_ACQ_TIMEOUT_MS) {
            out[0] = 0u; out[1] = 0u; return 0u;   /* 防卡死 */
        }
    }
    out[0] = s_pd_raw[0];
    out[1] = s_pd_raw[1];
    return 1u;
}
#endif  /* H_LOCK_ANALOG_PID */

uint16_t SigAcq_capture(uint16_t *buf, uint16_t n)
{
    uint16_t words = (uint16_t)(n >> 1);   /* FIFO 每字含 2 样本 */
    uint32_t t0;

#if H_ACQ_USE_TIMER_TRIGGER
    acq_timer_stop();
#endif
    s_dma_done = 0U;
    adc_flush(H_ADC_INST);                 /* ACQ-4: 冲掉上一轮残留再启动 */

    DL_DMA_setSrcAddr(DMA, H_DMA_CHAN, DL_ADC12_getFIFOAddress(H_ADC_INST));
    DL_DMA_setDestAddr(DMA, H_DMA_CHAN, (uint32_t)buf);
    DL_DMA_setTransferSize(DMA, H_DMA_CHAN, words);
    DL_DMA_enableChannel(DMA, H_DMA_CHAN);

    DL_ADC12_enableConversions(H_ADC_INST);
#if H_ACQ_USE_TIMER_TRIGGER
    acq_timer_start();                     /* 定时器事件驱动转换(不需 startConversion) */
#else
    DL_ADC12_startConversion(H_ADC_INST);
#endif

    t0 = Tick;
    while (s_dma_done == 0U) {
        __WFE();
        if ((uint32_t)(Tick - t0) > (uint32_t)H_ACQ_TIMEOUT_MS) { break; }  /* 防卡死 */
    }

    DL_ADC12_stopConversion(H_ADC_INST);
#if H_ACQ_USE_TIMER_TRIGGER
    acq_timer_stop();
#endif
    return n;
}

/* 双路同步采集: C -> c_buf (ADC0), 反馈 A'+B' -> fb_buf (ADC1).
 * 两路同窗口采样, 使 (C相位 − 反馈相位) 差分干净, 供 PLL 鉴相.
 *
 * 定时器模式(H_ACQ_USE_TIMER_TRIGGER=1, 推荐): "停表 -> flush两路 -> 装填两路DMA ->
 *   两路都 enableConversions -> 启表". 停表期间使能两路, 杜绝"使能间隙落入一个定时器边沿
 *   导致两路错开一个采样点(~975ns)"的竞态; 启表后第一个 ZERO 事件【同时】命中 ADC0/ADC1,
 *   零时间差 + 块对齐. 发挥②相位差≤5° 对此敏感(一个采样点≈在100kHz上35°).
 * 软件模式(=0, 回退): 两路软件启动相隔数条指令(~百ns), 残差小且基本恒定, 可上板标定补偿. */
uint16_t SigAcq_captureDual(uint16_t *c_buf, uint16_t *fb_buf, uint16_t n)
{
    uint16_t words = (uint16_t)(n >> 1);
    uint32_t t0;

#if H_ACQ_USE_TIMER_TRIGGER
    acq_timer_stop();
#endif
    s_dma_done = 0U;
    s_fb_dma_done = 0U;
    adc_flush(H_ADC_INST);
    adc_flush(H_FB_INST);

    DL_DMA_setSrcAddr(DMA, H_DMA_CHAN, DL_ADC12_getFIFOAddress(H_ADC_INST));
    DL_DMA_setDestAddr(DMA, H_DMA_CHAN, (uint32_t)c_buf);
    DL_DMA_setTransferSize(DMA, H_DMA_CHAN, words);
    DL_DMA_enableChannel(DMA, H_DMA_CHAN);

    DL_DMA_setSrcAddr(DMA, H_FB_DMA_CHAN, DL_ADC12_getFIFOAddress(H_FB_INST));
    DL_DMA_setDestAddr(DMA, H_FB_DMA_CHAN, (uint32_t)fb_buf);
    DL_DMA_setTransferSize(DMA, H_FB_DMA_CHAN, words);
    DL_DMA_enableChannel(DMA, H_FB_DMA_CHAN);

    DL_ADC12_enableConversions(H_ADC_INST);
    DL_ADC12_enableConversions(H_FB_INST);
#if H_ACQ_USE_TIMER_TRIGGER
    acq_timer_start();   /* 第一个 ZERO 事件同时触发两路 ADC => 零时间差 */
#else
    DL_ADC12_startConversion(H_ADC_INST);
    DL_ADC12_startConversion(H_FB_INST);
#endif

    t0 = Tick;
    while ((s_dma_done == 0U) || (s_fb_dma_done == 0U)) {
        __WFE();
        if ((uint32_t)(Tick - t0) > (uint32_t)H_ACQ_TIMEOUT_MS) { break; }  /* 防卡死 */
    }

    DL_ADC12_stopConversion(H_ADC_INST);
    DL_ADC12_stopConversion(H_FB_INST);
#if H_ACQ_USE_TIMER_TRIGGER
    acq_timer_stop();
#endif
    return n;
}

/* ADC0 中断: C 路 DMA 搬完整块 -> 置标志 */
void ADC0_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(H_ADC_INST)) {
        case DL_ADC12_IIDX_DMA_DONE: s_dma_done = 1U; break;
        default: break;
    }
}

/* ADC1 中断: 数字模式=反馈块 DMA 完成; 模拟模式=鉴相 2 通道序列(MEM1)完成 */
void ADC1_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(H_FB_INST)) {
#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
        case DL_ADC12_IIDX_MEM1_RESULT_LOADED:
            s_pd_raw[0] = (uint16_t)DL_ADC12_getMemResult(H_FB_INST, DL_ADC12_MEM_IDX_0);
            s_pd_raw[1] = (uint16_t)DL_ADC12_getMemResult(H_FB_INST, DL_ADC12_MEM_IDX_1);
            s_pd_done   = 1U;
            DL_ADC12_enableConversions(H_FB_INST);   /* 重新使能下次单次转换 */
            break;
#else
        case DL_ADC12_IIDX_DMA_DONE: s_fb_dma_done = 1U; break;
#endif
        default: break;
    }
}
