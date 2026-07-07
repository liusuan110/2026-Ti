/**********************************************************
                       康威电子
										 
功能：stm32f103rct6控制，AD9959正弦波点频输出，范围0-200M，
			显示：12864cog
接口：
AD9959模块			STM32单片机接口
				CS			——PA6;     
				SCLK 		——PB1;   
				UPDATE	——PB0;	  
				SP0    	——PA7;   
				SP1			——PA2;     
				SP2			——PB10; 
				SP3			——PC0;     
				SDIO0		——PA5;     
				SDIO1		——PA4;     
				SDIO2		——PA3;     
				SDIO3		——PA8;     
AD9959_PWR(PDC)——PA9;     
			RST			——PA10;         
			GND			--GND(0V)  

时间：2023-xxx
版本：2.6
作者：康威电子
其他：本程序只供学习使用，盗版必究。

更多电子需求，请到淘宝店，康威电子竭诚为您服务 ^_^
https://kvdz.taobao.com/ 

**********************************************************/
#include "stm32_config.h"
#include "stdio.h"
#include "led.h"
#include "lcd.h"
#include "AD9959.h"
#include "key.h"
#include "timer.h"
#include "task_manage.h"

extern u8 _return;
int main(void)
{
	
	MY_NVIC_PriorityGroup_Config(NVIC_PriorityGroup_2);	//设置中断分组
	delay_init(72);	//初始化延时函数
	LED_Init();	//初始化LED接口
	key_init();
	AD9959_Init();
	Timerx_Init(99,71);
	initial_lcd();
  delay_ms(300);
  
		//1、
	//后续代码涉及界面及按键交互功能，频率或幅度或其他参数可能被重写，可能导致在此处更改参数无效
	//上述情况，取消下面注释即可，可直接更改频率及幅度，
//	AD9959_Init();								//初始化控制AD9959需要用到的IO口,及寄存器
//	AD9959_Set_Fre(CH0, 100000);	//设置通道0频率100000Hz
//	AD9959_Set_Fre(CH1, 100000);	//设置通道1频率100000Hz
//	AD9959_Set_Fre(CH2, 100000);	//设置通道2频率100000Hz
//	AD9959_Set_Fre(CH3, 100000);	//设置通道3频率100000Hz
//		
//	AD9959_Set_Amp(CH0, 1023); 		//设置通道0幅度控制值1023，范围0~1023
//	AD9959_Set_Amp(CH1, 1023); 		//设置通道1幅度控制值1023，范围0~1023
//	AD9959_Set_Amp(CH2, 1023); 		//设置通道2幅度控制值1023，范围0~1023
//	AD9959_Set_Amp(CH3, 1023); 		//设置通道3幅度控制值1023，范围0~1023

//	AD9959_Set_Phase(CH0, 0);			//设置通道0相位控制值0(0度)，范围0~16383
//	AD9959_Set_Phase(CH1, 4096);	//设置通道1相位控制值4096(90度)，范围0~16383
//	AD9959_Set_Phase(CH2, 8192);	//设置通道2相位控制值8192(180度)，范围0~16383
//	AD9959_Set_Phase(CH3, 12288);	//设置通道3相位控制值12288(270度)，范围0~16383
//	IO_Update();	//AD9959更新数据,调用此函数后，上述操作生效！！！！
//	while(1);
	
	//2、	
//	关于扫频的说明
//	该程序的扫频功能利用定时器中断，不断写入自加的频率实现，
//	在timer.C的TIM4_IRQHandler中将自加后的频率写入
	
	while(1)
	{
		KeyRead();
		Set_PointFre(Keycode, 0);
		if(_return)
		{
			_return=0;
			LCD_Refresh_Gram();
		}
		KEY_EXIT();
	}	
}

