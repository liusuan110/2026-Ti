#include "adc_measure.h"
#include "config.h"

void ADC_init(void) {
    /* ��һͨ·: P1.0(A0) ��פģ������ */
    ADC10AE0 = ADC_PIN_ALL;
}

uint16_t ADC_readSingle(void) {
    uint16_t val;
    ADC10CTL0 &= ~ENC; 
    /* ���ü�����ٵ���̲���ʱ��: 4 x ADC_CLK */
    ADC10CTL0 = SREF_0 | ADC10SHT_0 | ADC10ON;
    /* ʱ��Դ: ADC�ڲ�����, ָ�����β��� A0 */
    ADC10CTL1 = ADC_CH_ALL | ADC10DIV_0 | ADC10SSEL_0 | CONSEQ_0; 

    /* ����ת�� */
    ADC10CTL0 |= ENC | ADC10SC;
    while (ADC10CTL1 & ADC10BUSY);
    val = ADC10MEM;
    ADC10CTL0 &= ~ENC;
    return val;
}

uint16_t ADC_measureVDC(void) {
    uint32_t sum = 0;
    uint8_t i;
    for (i = 0; i < 64; i++) {
        sum += ADC_readSingle();
    }
    sum >>= 6; // ��ֵ
    return (uint16_t)ADC_TO_MV(sum);
}

void ADC_sampleToBufferAdaptive(int16_t *buf, uint16_t len) {
    uint16_t i;
    uint16_t first_val = ADC_readSingle();
    
    /* ���ױ��ش����ȴ�: �ȴ��ϴ���ֵ���ߵȳ�ʱ */
    uint16_t timeout = 50000;
    while(timeout--) {
        uint16_t v = ADC_readSingle();
        if (first_val < 512 && v >= 512) break; /* ������ */
        first_val = v;
    }

    ADC10CTL0 &= ~ENC;
    ADC10CTL0 = SREF_0 | ADC10SHT_0 | ADC10ON;
    ADC10CTL1 = ADC_CH_ALL | ADC10DIV_0 | ADC10SSEL_0;

    for (i = 0; i < len; i++) {
        ADC10CTL0 |= ENC | ADC10SC;
        while (ADC10CTL1 & ADC10BUSY);
        buf[i] = (int16_t)ADC10MEM;
        ADC10CTL0 &= ~ENC;
        
        /* ����ʱ���Ʋ����� */
        __delay_cycles(100);
    }
}
