#include "GPIO.h"

/**
 * @brief 设置GPIO引脚模式
 * @param pin 要设置的引脚
 * @param mode 模式(输入, 带上拉的输入, 带下拉的输入, 输出)
*/
void GPIO_pinMode(uint16_t pin, uint8_t mode) {
    if (pin <= P1_7) { // 引脚在P1端口
        P1SEL &= ~pin; // 选择pin引脚GPIO模式
        P1SEL2 &= ~pin;
        switch (mode) {
            case INPUT:
                P1DIR &= ~pin; // 将pin引脚设置为输入
                P1REN &= ~pin; // 禁用pin引脚的电阻
                break;
            case INPUT_PULLUP:
                P1DIR &= ~pin; // 将pin引脚设置为输入
                P1REN |= pin;  // 启用pin引脚的电阻
                P1OUT |= pin;  // 将pin引脚电阻设置为上拉
                break;
            case INPUT_PULLDOWN:
                P1DIR &= ~pin; // 将pin引脚设置为输入
                P1REN |= pin;  // 启用pin引脚的电阻
                P1OUT &= ~pin; // 将pin引脚电阻设置为下拉
                break;
            case OUTPUT:
                P1DIR |= pin;  // 将pin引脚设置为输出
                P1OUT &= ~pin; // 将pin引脚输出设置为低电平
                break;
        }
    }
    else { // 引脚在P2端口
        pin -= BIT7;
        P2SEL &= ~pin; // 选择pin引脚GPIO模式
        P2SEL2 &= ~pin;
        switch (mode) {
            case INPUT:
                P2DIR &= ~pin; // 将pin引脚设置为输入
                P2REN &= ~pin; // 禁用pin引脚的电阻
                break;
            case INPUT_PULLUP:
                P2DIR &= ~pin; // 将pin引脚设置为输入
                P2REN |= pin;  // 启用pin引脚的电阻
                P2OUT |= pin;  // 将pin引脚电阻设置为上拉
                break;
            case INPUT_PULLDOWN:
                P2DIR &= ~pin; // 将pin引脚设置为输入
                P2REN |= pin;  // 启用pin引脚的电阻
                P2OUT &= ~pin; // 将pin引脚电阻设置为下拉
                break;
            case OUTPUT:
                P2DIR |= pin;  // 将pin引脚设置为输出
                P2OUT &= ~pin; // 将pin引脚输出设置为低电平
                break;
        }
    }
}

/**
 * @brief 读取GPIO引脚电平
 * @param pin 要读取的引脚
 * @return 引脚电平(高电平或低电平)
*/
uint8_t GPIO_pinRead(uint16_t pin) {
    if (pin <= P1_7) { // 引脚在P1端口
        if (P1IN & pin) {
            return HIGH;
        }
        else {
            return LOW;
        }
    }
    else { // 引脚在P2端口
        pin -= BIT7;
        if (P2IN & pin) {
            return HIGH;
        }
        else {
            return LOW;
        }
    }
}

/**
 * @brief 设置GPIO引脚输出电平
 * @param pin 要输出的引脚
 * @param val 输出电平(高电平或低电平)
*/
void GPIO_pinWrite(uint16_t pin, uint8_t val) {
    if (pin <= P1_7) { // 引脚在P1端口
        if (val == LOW) {
            P1OUT &= ~pin;
        }
        else {
            P1OUT |= pin;
        }
    }
    else { // 引脚在P2端口
        pin -= BIT7;
        if (val == LOW) {
            P2OUT &= ~pin;
        }
        else {
            P2OUT |= pin;
        }
    }
}
