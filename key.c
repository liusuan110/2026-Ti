/*
 * key.c
 * 按键驱动实现 (消抖 + 边沿检测)
 */

#include "key.h"
#include "GPIO.h"

/* 按键事件标志 (ISR置位, 主循环读取并清零) */
static volatile uint8_t g_key_event_flags = 0;

/* 初始化: P1.3 设为内部上拉输入 + 端口中断 */
void Key_init(void)
{
    /* P1.3 复用为按键 GPIO */
    P1SEL  &= ~KEY1_BIT;
    P1SEL2 &= ~KEY1_BIT;
    P1DIR  &= ~KEY1_BIT;  /* 输入 */
    P1REN  |=  KEY1_BIT;  /* 使能上拉/下拉 */
    P1OUT  |=  KEY1_BIT;  /* 上拉 */

    /* 按键中断: 高->低沿触发 (按下触发) */
    P1IES  |=  KEY1_BIT;
    P1IFG  &= ~KEY1_BIT;
    P1IE   |=  KEY1_BIT;
}

uint8_t Key_getEvent(void)
{
    uint8_t flags;

    /* 8位读写本身原子, 关中断仅用于防止读清零窗口竞态 */
    __bic_SR_register(GIE);
    flags = g_key_event_flags;
    g_key_event_flags = 0;
    __bis_SR_register(GIE);

    if (flags & KEY_1_SHORT) return KEY_1_SHORT;
    return KEY_NONE;
}

/*
 * 按键扫描: 带消抖的短按检测
 * 调用间隔建议 20~50ms
 * 返回值: KEY_NONE / KEY_1_SHORT / KEY_2_SHORT
 */
uint8_t Key_scan(void)
{
    return Key_getEvent();
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1_ISR(void)
{
    if (P1IFG & KEY1_BIT) {
        __delay_cycles(160000); /* 16MHz下约10ms消抖 */
        if ((P1IN & KEY1_BIT) == 0) {
            g_key_event_flags |= KEY_1_SHORT;
        }
        P1IFG &= ~KEY1_BIT;
    }
}
