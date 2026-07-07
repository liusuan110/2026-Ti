#ifndef __USER_UART_H__
#define __USER_UART_H__

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "ti_msp_dl_config.h"

#define UART_TX_BUF_SIZE 256

int UART0_sendStr(const char* str);
int UART2_sendStr(const char* str);
void UART0_printf(char* fmt, ...);
void UART2_printf(char* fmt, ...);

#endif /* #ifndef __USER_UART_H__ */
