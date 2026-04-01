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

/* ============ 内部状态 ============ */

/* 频率捕获: 记录上一次上升沿的 CCR0 值 */
static volatile uint16_t cap0_prev  = 0;
static volatile uint8_t  cap0_first = 1; /* 首次捕获标志 */

/* 频率中值滤波环形缓冲区 */
static volatile uint16_t prd_ring[FREQ_MED_SIZE];
static volatile uint8_t  prd_wr  = 0;
static volatile uint8_t  prd_cnt = 0;

/*
 * 小数组插入排序取中值 (最多 FREQ_MED_SIZE=5 个元素, ISR 内开销极小)
 */
static uint16_t period_median(void)
{
    uint16_t a[FREQ_MED_SIZE];
    uint8_t n = prd_cnt;
    uint8_t i, j;
    uint16_t tmp;

    for (i = 0; i < n; i++) a[i] = prd_ring[i];

    /* 插入排序 */
    for (i = 1; i < n; i++) {
        tmp = a[i];
        j = i;
        while (j > 0 && a[j - 1] > tmp) {
            a[j] = a[j - 1];
            j--;
        }
        a[j] = tmp;
    }
    return a[n / 2];
}

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
    prd_wr  = 0;
    prd_cnt = 0;

    /* CCR0: 上升沿捕获, CCI0A (TA0.0), 同步, 开中断 */
    TA0CCTL0 = CM_1 | CCIS_0 | SCS | CAP | CCIE;

    /* 连续模式启动 */
    TA0CTL = TASSEL_2 | ID_0 | MC_2 | TACLR;
}

void Capture_stopFreq(void)
{
    TA0CCTL0 = 0; /* 关闭 CCR0 捕获和中断 */
}

void Capture_stopAll(void)
{
    TA0CCTL0 = 0;
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
    } else {
        /* 周期 = 当前值 - 上次值 (自动处理16位溢出) */
        uint16_t period = cap_val - cap0_prev;
        cap0_prev = cap_val;

        if (period > 0) {
            uint16_t med;
            /* 将周期存入环形缓冲区, 取中值后计算频率 */
            prd_ring[prd_wr] = period;
            if (++prd_wr >= FREQ_MED_SIZE) prd_wr = 0;
            if (prd_cnt < FREQ_MED_SIZE) prd_cnt++;

            med = period_median();
            g_freq_hz = SYS_FREQ / (uint32_t)med;
            g_freq_period = med; /* 用中值而非原始值, 避免ETS相位抖动 */
            g_freq_ready = 1;
        }
    }
}
