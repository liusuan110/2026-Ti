/*
 * uart.c
 * UART communication implementation
 */

#include "uart.h"
#include <msp430g2553.h>

void UART_init(void)
{
    /* Use P1.2 as TXD. P1.1 is RXD, but currently used as Timer Capture */
    P1SEL |= BIT2;   
    P1SEL2 |= BIT2;  

    UCA0CTL1 |= UCSWRST;      /* Put USCI in reset */
    UCA0CTL1 |= UCSSEL_2;     /* SMCLK (16MHz) */
    
    /* 16MHz / 9600 = 1666.66 => 1666 */
    /* Modulation: 0.66 * 8 = 5 => UCBRSx = 5 */
    // UCA0BR1 = 1666 / 256 = 6; UCA0BR0 = 1666 % 256 = 130
    UCA0BR0 = 130;
    UCA0BR1 = 6;
    UCA0MCTL = UCBRS_5;
    
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