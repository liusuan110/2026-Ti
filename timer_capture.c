#include "timer_capture.h"
#include "config.h"

volatile uint32_t g_freq_hz = 0;
volatile uint32_t g_duty = 0; 

static volatile uint16_t t1 = 0;
static volatile uint16_t t2 = 0;
static volatile uint16_t t3 = 0;

void Capture_init(void) {
    P1DIR &= ~CAP_FREQ_PIN;
    P1SEL |= CAP_FREQ_PIN;
    P1SEL2 &= ~CAP_FREQ_PIN;
    
    // Timer0_A config: SMCLK (16MHz), Continuous mode
    TA0CTL = TASSEL_2 | MC_2 | TACLR; 
}

void Capture_start(void) {
    // 纯多通道测量的降维保命法：绝对不用中断！只配捕获功能，后续我们纯靠轮询
    // 彻底切断任何因为大频率信号带来的 CPU 中断风暴死机现象
    TA0CCTL0 = CM_1 | CCIS_0 | SCS | CAP; // 没有 CCIE !
    TA0CTL &= ~TACLR;
}

void Capture_stop(void) {
    TA0CCTL0 = 0; 
}

/* 改为主动轮询测量 - 防死机、防卡死、防风暴 */
void Capture_poll(void) {
    uint32_t timeout;
    
    // T1: 等待上升沿
    TA0CCTL0 = CM_1 | CCIS_0 | SCS | CAP;
    TA0CCTL0 &= ~CCIFG;
    timeout = 100000;
    while(!(TA0CCTL0 & CCIFG) && timeout) timeout--;
    if (timeout == 0) { g_freq_hz = 0; g_duty = 0; return; }
    t1 = TA0CCR0;
    
    // T2: 等待下降沿
    TA0CCTL0 = CM_2 | CCIS_0 | SCS | CAP;
    TA0CCTL0 &= ~CCIFG;
    timeout = 100000;
    while(!(TA0CCTL0 & CCIFG) && timeout) timeout--;
    if (timeout == 0) { g_freq_hz = 0; g_duty = 0; return; }
    t2 = TA0CCR0;

    // T3: 等待再次上升沿
    TA0CCTL0 = CM_1 | CCIS_0 | SCS | CAP;
    TA0CCTL0 &= ~CCIFG;
    timeout = 100000;
    while(!(TA0CCTL0 & CCIFG) && timeout) timeout--;
    if (timeout == 0) { g_freq_hz = 0; g_duty = 0; return; }
    t3 = TA0CCR0;

    // 计算解算
    uint32_t period = (t3 >= t1) ? (t3 - t1) : (0x10000UL - t1 + t3);
    uint32_t high_time = (t2 >= t1) ? (t2 - t1) : (0x10000UL - t1 + t2);
    
    if (period > 0) {
        g_freq_hz = SYS_FREQ / period;
        g_duty = (high_time * 100) / period;
    }
}
