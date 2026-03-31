#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdint.h>
#include <msp430g2553.h>

// GPIO状态
#define LOW  0
#define HIGH 1

// GPIO模式
#define INPUT          0 // 输入
#define INPUT_PULLUP   1 // 带上拉的输入
#define INPUT_PULLDOWN 2 // 带下拉的输入
#define OUTPUT         3 // 输出

// 引脚对应位数
#define P1_0 BIT0
#define P1_1 BIT1
#define P1_2 BIT2
#define P1_3 BIT3
#define P1_4 BIT4
#define P1_5 BIT5
#define P1_6 BIT6
#define P1_7 BIT7
#define P2_0 (BIT0 + BIT7)
#define P2_1 (BIT1 + BIT7)
#define P2_2 (BIT2 + BIT7)
#define P2_3 (BIT3 + BIT7)
#define P2_4 (BIT4 + BIT7)
#define P2_5 (BIT5 + BIT7)
#define P2_6 (BIT6 + BIT7)
#define P2_7 (BIT7 + BIT7)

/**
 * @brief 设置GPIO引脚模式
 * @param pin 要设置的引脚
 * @param mode 模式(输入, 带上拉的输入, 带下拉的输入, 输出)
*/
void GPIO_pinMode(uint16_t pin, uint8_t mode);

/**
 * @brief 读取GPIO引脚电平
 * @param pin 要读取的引脚
 * @return 引脚电平(高电平或低电平)
*/
uint8_t GPIO_pinRead(uint16_t pin);

/**
 * @brief 设置GPIO引脚输出电平
 * @param pin 要输出的引脚
 * @param val 输出电平(高电平或低电平)
*/
void GPIO_pinWrite(uint16_t pin, uint8_t val);

#endif /* #ifndef __GPIO_H__ */
