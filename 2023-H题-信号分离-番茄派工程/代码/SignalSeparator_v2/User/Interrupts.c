#include "Interrupts.h"

// SysTick中断服务函数(1ms)
void SysTick_Handler(void) {
    Tick_SysTickCallback();
}

// ADC0中断服务函数
void ADC0_IRQHandler(void) {
    UserADC0_IRQCallback();
}

// ADC1中断服务函数
void ADC1_IRQHandler(void) {
    UserADC1_IRQCallback();
}

// GPIO中断服务函数
void GROUP1_IRQHandler(void) {
    GROUP1_IRQCallback();
}
