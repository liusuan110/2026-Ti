#include "UserADC.h"

volatile uint8_t ADC0DataValid = 0;
volatile uint16_t ADC1Data[2] = {0};
volatile uint8_t ADC1DataValid = 0;

void UserADC_init(void) {
    NVIC_EnableIRQ(ADC1_INST_INT_IRQN);
}

/**
 * @brief 设置ADC事件定时器
 * @param fs 采样率(Sa/s)
 * @return 实际采样率(Sa/s)
 */
float UserADC_setEventTimer(uint32_t fs) {
    uint32_t loadVal;
    loadVal = (uint32_t)(80E6F / (float)fs - 0.5F); // 事件定时器时钟频率80MHz
    DL_TimerG_setLoadValue(ADC_EVENT_TIMER, loadVal);
    return (80E6F / (float)(loadVal + 1));
}

void UserADC0_startDMA(uint16_t* dst, uint16_t count) {
    ADC0DataValid = 0;
    DL_DMA_setSrcAddr(DMA, DMA_ADC0_CHAN_ID, DL_ADC12_getMemResultAddress(ADC0_INST, ADC0_ADCMEM_0));
    DL_DMA_setDestAddr(DMA, DMA_ADC0_CHAN_ID, (uint32_t)dst);
    DL_DMA_setTransferSize(DMA, DMA_ADC0_CHAN_ID, count);
    DL_DMA_enableChannel(DMA, DMA_ADC0_CHAN_ID);
    NVIC_EnableIRQ(ADC0_INST_INT_IRQN);
    DL_ADC12_enableConversions(ADC0_INST);
    delay_cycles(800);
    DL_TimerG_startCounter(ADC_EVENT_TIMER);
}

void UserADC1_start(void) {
    ADC1DataValid = 0;
    DL_ADC12_startConversion(ADC1_INST);
}

UserADC_Status_t UserADC0_waitForConvertion(uint32_t timeout) {
    uint32_t startTime = Tick;
    while (!ADC0DataValid) {
        if (Tick - startTime >= timeout) return USER_ADC_STA_TIMEOUT;
    }
    return USER_ADC_STA_DONE;
}

uint16_t UserADC1_getData(uint16_t mem, uint32_t timeout) {
    uint32_t startTime = Tick;
    while (!ADC1DataValid) {
        if (Tick - startTime >= timeout) return 0;
    }
    return ADC1Data[mem];
}

float UserADC1_getVoltage(uint16_t mem, uint32_t timeout) {
    uint32_t startTime = Tick;
    while (!ADC1DataValid) {
        if (Tick - startTime >= timeout) return 0;
    }
    return ADC_VAL_TO_VOLTAGE(ADC1Data[mem]);
}

// ADC0中断回调
void UserADC0_IRQCallback(void) {
    if (DL_ADC12_getPendingInterrupt(ADC0_INST) == DL_ADC12_IIDX_DMA_DONE) {
        DL_TimerG_stopCounter(ADC_EVENT_TIMER);
        ADC0DataValid = 1;
    }
}

// ADC1中断回调
void UserADC1_IRQCallback(void) {
    if (DL_ADC12_getPendingInterrupt(ADC1_INST) == DL_ADC12_IIDX_MEM1_RESULT_LOADED) {
        ADC1Data[0] = DL_ADC12_getMemResult(ADC1_INST, ADC1_ADCMEM_0);
        ADC1Data[1] = DL_ADC12_getMemResult(ADC1_INST, ADC1_ADCMEM_1);
        ADC1DataValid = 1;
        DL_ADC12_enableConversions(ADC1_INST);
    }
}
