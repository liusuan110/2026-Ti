#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <msp430g2553.h>

#define SYS_FREQ        16000000UL

#define ADC_CH_ALL      INCH_0      
#define ADC_PIN_ALL     BIT0        

#define CAP_FREQ_PIN    BIT1        

#define KEY1_BIT        BIT3        

typedef enum {
    PAGE_INFO = 0,   
    PAGE_VDC,        
    PAGE_DECODE,     
    PAGE_FREQ_DUTY,  
    PAGE_WAVE,       
    PAGE_SPECTRUM,   
    PAGE_COUNT
} PageID;

#define WAVE_RAW_POINTS 128         
#define FFT_N           32          
#define FFT_LOG2N       5

#define ADC_VREF_MV     3300        
#define ADC_TO_MV(val)  ((int32_t)(val) * ADC_VREF_MV / 1023)

#endif 
