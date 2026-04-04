#ifndef __TIMER_CAPTURE_H__
#define __TIMER_CAPTURE_H__

#include <msp430g2553.h>
#include <stdint.h>

extern volatile uint32_t g_freq_hz;
extern volatile uint32_t g_duty;

void Capture_init(void);
void Capture_start(void);
void Capture_stop(void);
void Capture_poll(void);

#endif 
