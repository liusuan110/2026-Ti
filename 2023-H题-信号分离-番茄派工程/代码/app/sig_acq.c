#include "sig_acq.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>

/* 番茄派 ADC 输入通道 —— TODO: 按你板子丝印/接线确认
 * (官方例程: CHAN_2=PA25, CHAN_4=PB25; 番茄派引出哪个看实物). */
#define H_ADC_INST   ADC0
#define H_ADC_CHAN   DL_ADC12_INPUT_CHAN_2
#define H_ADC_MEM    DL_ADC12_MEM_IDX_0

static const DL_ADC12_ClockConfig kAdcClk = {
    .clockSel    = DL_ADC12_CLOCK_ULPCLK,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_1,
    .freqRange   = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};

void SigAcq_init(void)
{
    DL_ADC12_setClockConfig(H_ADC_INST, (DL_ADC12_ClockConfig *)&kAdcClk);

    /* 单通道, 软件触发 (骨架). 参考电压 VDDA(3.3V) */
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

    DL_ADC12_setPowerDownMode(H_ADC_INST, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(H_ADC_INST, 16U);
    DL_ADC12_enableConversions(H_ADC_INST);
}

uint16_t SigAcq_capture(uint16_t *buf, uint16_t n)
{
    uint16_t i;
    volatile uint32_t d;

    for (i = 0U; i < n; i++) {
        DL_ADC12_startConversion(H_ADC_INST);
        /* 骨架: 简易等待转换完成. 上板换成 DMA+定时器触发, 这里只为跑通流程 */
        for (d = 0U; d < 64U; d++) { __asm volatile("nop"); }
        buf[i] = DL_ADC12_getMemResult(H_ADC_INST, H_ADC_MEM);
        DL_ADC12_enableConversions(H_ADC_INST);
    }
    return n;
}
