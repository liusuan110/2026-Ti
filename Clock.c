#include "Clock.h"

/**
 * @brief 初始化系统时钟
 * @note 在宏定义中修改时钟频率
*/
void Clock_init() {
    switch (CPU_CLOCK) {
        case F_1MHz:
            if (CALBC1_1MHZ == 0xFF) { // If calibration constant erased
                while (1);             // do not load, trap CPU!!
            }
            DCOCTL = 0;                // Select lowest DCOx and MODx settings
            BCSCTL1 = CALBC1_1MHZ;     // Set range
            DCOCTL = CALDCO_1MHZ;      // Set DCO step + modulation
            break;
        case F_8MHz:
            if (CALBC1_8MHZ == 0xFF) { // If calibration constant erased
                while (1);             // do not load, trap CPU!!
            }
            DCOCTL = 0;                // Select lowest DCOx and MODx settings
            BCSCTL1 = CALBC1_8MHZ;     // Set range
            DCOCTL = CALDCO_8MHZ;      // Set DCO step + modulation
            break;
        case F_12MHz:
            if (CALBC1_12MHZ == 0xFF) { // If calibration constant erased
                while (1);              // do not load, trap CPU!!
            }
            DCOCTL = 0;                 // Select lowest DCOx and MODx settings
            BCSCTL1 = CALBC1_12MHZ;     // Set range
            DCOCTL = CALDCO_12MHZ;      // Set DCO step + modulation
            break;
        case F_16MHz:
            if (CALBC1_16MHZ == 0xFF) { // If calibration constant erased
                while (1);              // do not load, trap CPU!!
            }
            DCOCTL = 0;                 // Select lowest DCOx and MODx settings
            BCSCTL1 = CALBC1_16MHZ;     // Set range
            DCOCTL = CALDCO_16MHZ;      // Set DCO step + modulation
            break;
    }
}

/**
 * @brief 延时(毫秒)
 * @param t 需要延时的时间(毫秒)
*/
void delay(uint32_t t) {
    while(t > 0) {
        __delay_cycles(CYCLES_PER_MICROSECOND * 1000);
        t--;
    }
}
