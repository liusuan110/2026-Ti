#ifndef __USER_ADC_H__
#define __USER_ADC_H__

#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "Tick.h"

#define ADC_VREF 3.3F // ADC基准电压
// ADC值转为电压(V)
#define ADC_VAL_TO_VOLTAGE(x) ((float)(x) * ADC_VREF / 4096)
#define IS_ADC0_DONE (ADC0DataValid)
#define IS_ADC1_DONE (ADC1DataValid)

#define ADC_EVENT_TIMER TIMER_ADC_INST

typedef enum UserADC_Status_t {
    USER_ADC_STA_TIMEOUT,
    USER_ADC_STA_DONE
} UserADC_Status_t;

extern volatile uint8_t ADC0DataValid;
extern volatile uint8_t ADC1DataValid;

void UserADC_init(void);
float UserADC_setEventTimer(uint32_t fs);
void UserADC0_startDMA(uint16_t* dst, uint16_t count);
void UserADC1_start(void);
UserADC_Status_t UserADC0_waitForConvertion(uint32_t timeout);
uint16_t UserADC1_getData(uint16_t mem, uint32_t timeout);
float UserADC1_getVoltage(uint16_t mem, uint32_t timeout);
void UserADC0_IRQCallback(void);
void UserADC1_IRQCallback(void);

#endif /* #ifndef __USER_ADC_H__ */
