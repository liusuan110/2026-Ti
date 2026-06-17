#include "sig_acq.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>

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

static volatile uint8_t s_dma_done    = 0U;
static volatile uint8_t s_fb_dma_done = 0U;

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
        DL_ADC12_TRIG_SRC_SOFTWARE, DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(inst, mem, chan, DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
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

uint16_t SigAcq_capture(uint16_t *buf, uint16_t n)
{
    uint16_t words = (uint16_t)(n >> 1);   /* FIFO 每字含 2 样本 */

    s_dma_done = 0U;
    adc_flush(H_ADC_INST);                 /* ACQ-4: 冲掉上一轮残留再启动 */

    DL_DMA_setSrcAddr(DMA, H_DMA_CHAN, DL_ADC12_getFIFOAddress(H_ADC_INST));
    DL_DMA_setDestAddr(DMA, H_DMA_CHAN, (uint32_t)buf);
    DL_DMA_setTransferSize(DMA, H_DMA_CHAN, words);
    DL_DMA_enableChannel(DMA, H_DMA_CHAN);

    DL_ADC12_enableConversions(H_ADC_INST);
    DL_ADC12_startConversion(H_ADC_INST);

    while (s_dma_done == 0U) { __WFE(); }

    DL_ADC12_stopConversion(H_ADC_INST);
    return n;
}

/* 双路近同步采集: C -> c_buf (ADC0), 反馈 A'+B' -> fb_buf (ADC1).
 * 两路同窗口采样, 使 (C相位 − 反馈相位) 差分干净, 供 PLL 鉴相. */
uint16_t SigAcq_captureDual(uint16_t *c_buf, uint16_t *fb_buf, uint16_t n)
{
    uint16_t words = (uint16_t)(n >> 1);

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
    /* ★同步: 两路软件启动仅相隔数条指令(~百ns), 残留时间差小且基本恒定, 可上板标定补偿.
     * 若要零时间差(更优): 用同一 TIMG 周期事件经事件总线同时触发 ADC0+ADC1(pub/sub 同通道号),
     * 把上面两路的 TRIG_SRC 改成 EVENT —— 参考 MSPM0例程-双ADC同步采样与FFT/15_adc_dma_time_trigger,
     * 顺带把 ACQ-7 的精确采样率也一并解决. */
    DL_ADC12_startConversion(H_ADC_INST);
    DL_ADC12_startConversion(H_FB_INST);

    while ((s_dma_done == 0U) || (s_fb_dma_done == 0U)) { __WFE(); }

    DL_ADC12_stopConversion(H_ADC_INST);
    DL_ADC12_stopConversion(H_FB_INST);
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

/* ADC1 中断: 反馈路 DMA 搬完整块 -> 置标志 */
void ADC1_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(H_FB_INST)) {
        case DL_ADC12_IIDX_DMA_DONE: s_fb_dma_done = 1U; break;
        default: break;
    }
}
