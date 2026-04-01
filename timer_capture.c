/*
 * timer_capture.c
 * Timer_A0 捕获模块实现
 *
 * Timer_A0 连续模式, SMCLK = 16MHz
 * CCR0 (TA0.0 / P1.1): 上升沿捕获 -> 频率
 */

#include "timer_capture.h"

/* ============ 全局测量结果 ============ */
volatile uint32_t g_freq_hz      = 0;
volatile uint8_t  g_freq_ready   = 0;
volatile uint16_t g_freq_period  = 0;

#define FREQ_ACCUM_EDGES 16 /* 连续捕获的边沿数，用于多周期累计平均 */

/* ============ 内部状态 ============ */

static volatile uint16_t cap0_prev  = 0;
static volatile uint8_t  cap0_first = 1; /* 首次捕获标志 */

static volatile uint16_t edge_count = 0;
static volatile uint32_t period_sum = 0;

/* ============ 初始化 ============ */

void Capture_init(void)
{
    /* 停止 Timer */
    TA0CTL = TASSEL_2 | ID_0 | MC_0 | TACLR;
    /* TASSEL_2: SMCLK (16MHz)
     * ID_0:     不分频
     * MC_0:     停止
     * TACLR:    清零 TAR */

    /* P1.1 -> TA0.0 捕获功能 */
    P1DIR &= ~CAP_FREQ_PIN;
    P1SEL |= CAP_FREQ_PIN;
    P1SEL2 &= ~CAP_FREQ_PIN;
}

/* ============ 频率捕获控制 ============ */

void Capture_startFreq(void)
{
    cap0_first = 1;
    g_freq_ready = 0;
    edge_count = 0;
    period_sum = 0;
    
    TA0CTL = TASSEL_2 | ID_0 | MC_0 | TACLR;

    /* CCR0: 上升沿捕获, CCI0A (TA0.0), 同步, 开中断 */
    TA0CCTL0 = CM_1 | CCIS_0 | SCS | CAP | CCIE;
    TA0CCTL0 &= ~(CCIFG);

    /* 连续模式启动 */
    TA0CTL = TASSEL_2 | ID_0 | MC_2 | TACLR;
}

void Capture_stopFreq(void)
{
    TA0CCTL0 &= ~(CCIE | CCIFG); /* 关闭 CCR0 捕获和中断 */
}

void Capture_stopAll(void)
{
    TA0CCTL0 &= ~(CCIE | CCIFG);
    TA0CTL = MC_0; /* Timer 停止 */
}

/* ============================================================
 * 中断服务函数
 * ============================================================ */

/*
 * Timer_A0 CCR0 中断 (频率捕获)
 * 专用向量, 仅 CCR0 触发
 */
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR(void)
{
    uint16_t cap_val = TA0CCR0;

    if (cap0_first) {
        cap0_prev = cap_val;
        cap0_first = 0;
        edge_count = 0;
        period_sum = 0;
    } else {
        /* 周期 = 当前值 - 上次值 (自动处理16位溢出) */
        uint16_t period = cap_val - cap0_prev;
        cap0_prev = cap_val;

        if (period > 0) {
            period_sum += period;
            edge_count++;

            /* 多周期累计法，满一定数量后计算平均周期 */
            if (edge_count >= FREQ_ACCUM_EDGES) {
                /* 累加跨度过大时可能溢出32位，但对于16MHz时钟，16个周期最大 16*65535 ~ 1000万，远小于42亿，安全。 */
                g_freq_period = (uint16_t)((period_sum + (FREQ_ACCUM_EDGES / 2)) / FREQ_ACCUM_EDGES);
                g_freq_ready = 1;
                
                /* 关闭捕获中断，防止32位除法计算时被高频中断卡死，并由主循环重新发起下一次测量 */
                TA0CCTL0 &= ~CCIE;
            }
        }
    }
}
