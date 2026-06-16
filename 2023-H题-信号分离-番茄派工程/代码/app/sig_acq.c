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

static volatile uint8_t s_dma_done = 0U;

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

void SigAcq_init(void)
{
    /* ADC 上电(不走 SysConfig, 手动 reset+enablePower) */
    DL_ADC12_reset(H_ADC_INST);
    DL_ADC12_enablePower(H_ADC_INST);
    delay_cycles(16);

    /* ADC 输入引脚设为模拟功能 */
    DL_GPIO_initPeripheralAnalogFunction(H_ADC_PIN_IOMUX);

    DL_ADC12_setClockConfig(H_ADC_INST, (DL_ADC12_ClockConfig *)&kAdcClk);

    /* 单通道 + repeat + 软件触发 */
    DL_ADC12_initSingleSample(H_ADC_INST,
        DL_ADC12_REPEAT_MODE_ENABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    DL_ADC12_configConversionMem(H_ADC_INST, H_ADC_MEM,
        H_ADC_CHAN, DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    DL_ADC12_enableFIFO(H_ADC_INST);
    DL_ADC12_setPowerDownMode(H_ADC_INST, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(H_ADC_INST, H_ADC_SAMPLE_TIME);

    /* DMA: ADC FIFO 阈值触发 */
    DL_ADC12_enableDMA(H_ADC_INST);
    DL_ADC12_setDMASamplesCnt(H_ADC_INST, 6);
    DL_ADC12_enableDMATrigger(H_ADC_INST, DL_ADC12_DMA_MEM10_RESULT_LOADED);

    DL_ADC12_clearInterruptStatus(H_ADC_INST, DL_ADC12_INTERRUPT_DMA_DONE);
    DL_ADC12_enableInterrupt(H_ADC_INST, DL_ADC12_INTERRUPT_DMA_DONE);

    /* DMA 通道配置 */
    DL_DMA_initChannel(DMA, H_DMA_CHAN, (DL_DMA_Config *)&kDmaCfg);

    DL_ADC12_enableConversions(H_ADC_INST);
    NVIC_EnableIRQ(H_ADC_INT_IRQN);
}

uint16_t SigAcq_capture(uint16_t *buf, uint16_t n)
{
    /* FIFO 每字含 2 样本 => DMA 搬 n/2 个字 */
    uint16_t words = (uint16_t)(n >> 1);

    s_dma_done = 0U;
    DL_DMA_setSrcAddr(DMA, H_DMA_CHAN, DL_ADC12_getFIFOAddress(H_ADC_INST));
    DL_DMA_setDestAddr(DMA, H_DMA_CHAN, (uint32_t)buf);
    DL_DMA_setTransferSize(DMA, H_DMA_CHAN, words);
    DL_DMA_enableChannel(DMA, H_DMA_CHAN);

    DL_ADC12_enableConversions(H_ADC_INST);
    DL_ADC12_startConversion(H_ADC_INST);

    /* 等 DMA 搬完一块 (DMA_DONE 中断置标志) */
    while (s_dma_done == 0U) {
        __WFE();
    }

    DL_ADC12_stopConversion(H_ADC_INST);
    return n;
}

/* ADC0 中断: DMA 搬完整块 -> 置完成标志 */
void ADC0_IRQHandler(void)
{
    switch (DL_ADC12_getPendingInterrupt(H_ADC_INST)) {
        case DL_ADC12_IIDX_DMA_DONE:
            s_dma_done = 1U;
            break;
        default:
            break;
    }
}
