#ifndef __INTERRUPTS_H__
#define __INTERRUPTS_H__

#include "ti_msp_dl_config.h"
#include "Tick.h"
#include "UserADC.h"
#include "Encoder.h"

void SysTick_Handler(void);
void ADC0_IRQHandler(void);
void ADC1_IRQHandler(void);
void GROUP1_IRQHandler(void);

#endif /* #ifndef __INTERRUPTS_H__ */
