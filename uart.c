/*
 * uart.c
 * UART communication implementation
 */

#include "uart.h"
#include <msp430g2553.h>

void UART_init(void)
{
    /* Use P1.2 as TXD. Pre-drive HIGH and set as output to avoid startup glitches */
    P1OUT |= BIT2;
    P1DIR |= BIT2;
    P1SEL |= BIT2;   
    P1SEL2 |= BIT2;  

    UCA0CTL1 |= UCSWRST;      /* Put USCI in reset */
    UCA0CTL0 = 0;             /* No parity, LSB first, 8-bit, 1 stop bit, UART */
    UCA0CTL1 |= UCSSEL_2;     /* SMCLK (16MHz) */
    
    /* 16MHz / 9600 = 1666.666; N = 1666 > 16, so highly recommended to use UCOS16=1 */
    /* Prescaler divided by 16: 1666 / 16 = 104 -> UCA0BR0=104, UCA0BR1=0 */
    /* Fraction: 0.1666 * 16 = 2.666 -> UCBRF=2, UCBRS=1 (from TI User Guide Table) */
    UCA0BR0 = 104;
    UCA0BR1 = 0;
    UCA0MCTL = 0x20 | 0x02 | UCOS16; /* UCBRF_2 (0x20) | UCBRS_1 (0x02) | UCOS16 */

    UCA0CTL1 &= ~UCSWRST;     /* Release USCI from reset */
}

void UART_sendChar(char c)
{
    while (!(IFG2 & UCA0TXIFG)); /* Wait until TX buffer is ready */
    UCA0TXBUF = c;
}

void UART_sendStr(const char *str)
{
    while (*str)
    {
        UART_sendChar(*str++);
    }
}

void UART_sendNum(uint32_t num)
{
    char buf[12];
    int i = 0;

    if (num == 0)
    {
        UART_sendChar('0');
        return;
    }

    while (num > 0)
    {
        buf[i++] = (char)(num % 10) + '0';
        num /= 10;
    }

    while (i > 0)
    {
        UART_sendChar(buf[--i]);
    }
}