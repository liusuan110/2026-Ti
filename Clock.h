#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <stdint.h>
#include <msp430g2553.h>

#define CPU_CLOCK F_16MHz // �ڴ˴��޸�ʱ��Ƶ��

#define F_1MHz 1000000
#define F_8MHz 8000000
#define F_12MHz 12000000
#define F_16MHz 16000000

#define CYCLES_PER_MICROSECOND CPU_CLOCK / F_1MHz


#ifndef CPU_CLOCK
#error CPU_CLOCK undefined!
#endif

#if ((CPU_CLOCK!=F_1MHz)&&(CPU_CLOCK!=F_8MHz)&&(CPU_CLOCK!=F_12MHz)&&(CPU_CLOCK!=F_16MHz))
#error CPU_CLOCK defined incorrect!
#endif

/**
 * @brief ��ʼ��ϵͳʱ��
 * @note �ں궨�����޸�ʱ��Ƶ��
*/
void Clock_init();

/**
 * @brief ��ʱ(����)
 * @param t ��Ҫ��ʱ��ʱ��(����)
*/
void delay(uint32_t t);

#endif /* #ifndef __CLOCK_H__ */
