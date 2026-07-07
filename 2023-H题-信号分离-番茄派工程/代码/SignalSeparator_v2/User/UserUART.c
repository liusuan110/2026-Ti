#include "UserUART.h"

int UART0_sendStr(const char* str) {
    int count = 0;
    while (*str) {
        DL_UART_transmitDataBlocking(UART_0_INST, (uint8_t)*str);
        str++;
        count++;
    }
    return count;
}

int UART2_sendStr(const char* str) {
    int count = 0;
    while (*str) {
        DL_UART_transmitDataBlocking(UART_2_INST, (uint8_t)*str);
        str++;
        count++;
    }
    return count;
}

void UART0_printf(char* fmt, ...) {
    static char buf[UART_TX_BUF_SIZE];
    int len;
    va_list args;
    va_start(args, fmt);
    len = vsprintf(buf, fmt, args);
    va_end(args);
    UART0_sendStr(buf);
}

void UART2_printf(char* fmt, ...) {
    static char buf[UART_TX_BUF_SIZE];
    int len;
    va_list args;
    va_start(args, fmt);
    len = vsprintf(buf, fmt, args);
    va_end(args);
    UART2_sendStr(buf);
}
