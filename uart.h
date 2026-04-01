/*
 * uart.h
 * UART communication module
 */

#ifndef UART_H_
#define UART_H_

#include <stdint.h>

void UART_init(void);
void UART_sendChar(char c);
void UART_sendStr(const char *str);
void UART_sendNum(uint32_t num);

#endif /* UART_H_ */