#ifndef __ADC_MEASURE_H__
#define __ADC_MEASURE_H__

#include <msp430g2553.h>
#include <stdint.h>

void ADC_init(void);
uint16_t ADC_readSingle(void);
uint16_t ADC_measureVDC(void);
void ADC_sampleToBufferAdaptive(int16_t *buf, uint16_t len);

#endif
