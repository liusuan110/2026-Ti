/**********************************************************
                       康威电子
功能：stm32f103rct6控制AD9959模块2电平二阶PSK调制模式
接口：控制引脚接口请参照AD9959.h
时间：
版本：2.6
作者：康威电子
其他：本程序只供学习使用，盗版必究。

更多电子需求，请到淘宝店，康威电子竭诚为您服务 ^_^
https://kvdz.taobao.com/ 
**********************************************************/

#include "stm32_config.h"
#include "stdio.h"
#include "AD9959.h"

int main(void)
{
	uint16_t PSK_phase_ch0[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint16_t PSK_phase_ch1[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint16_t PSK_phase_ch2[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint16_t PSK_phase_ch3[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//PSK数据
	MY_NVIC_PriorityGroup_Config(NVIC_PriorityGroup_2);	//设置中断分组
	delay_init(72);	//初始化延时函数
	delay_ms(500);//延时一会儿，等待上电稳定,确保AD9959比控制板先上电。
	
	//代码移植建议
	//1.修改头文件AD9959.h中，自己控制板实际需要使用哪些控制引脚。如CS脚改成PA0控制，则定义"#define CS			PAout(0)" 
	//2.修改C文件AD9959.c中，AD9959_Init函数，所有用到管脚的GPIO输出功能初始化
	//3.完成

	AD9959_Init();	//初始化控制AD9959需要用到的IO口,及寄存器
	AD9959_Modulation_Init(CH0|CH1|CH2|CH3,PSK,SWEEP_DISABLE,LEVEL_MOD_2);//通道0-3设置为PSK模式，不启用线性扫描，2电平调制
	PSK_phase_ch0[0]=0;//PSK相位1：0,范围：0~16383(对应角度：0°~360°)
	PSK_phase_ch0[1]=4095;//PSK相位2：90°
	AD9959_SetPSK(CH0,PSK_phase_ch0,1000);//设置PSK参数，CH0通道，P1=0°,P2=90°，频率1000hz
	
	PSK_phase_ch1[0]=0;//PSK相位1：0
	PSK_phase_ch1[1]=4095;//PSK相位2：90°
	AD9959_SetPSK(CH1,PSK_phase_ch1,1000);//设置PSK参数，CH1通道，P1=0°,P2=90°，频率1000hz
	
	PSK_phase_ch2[0]=0;//PSK相位1：0
	PSK_phase_ch2[1]=4095;//PSK相位2：90°
	AD9959_SetPSK(CH2,PSK_phase_ch2,1000);//设置PSK参数，CH2通道，P1=0°,P2=90°，频率1000hz
	
	PSK_phase_ch3[0]=0;//PSK相位1：0
	PSK_phase_ch3[1]=4095;//PSK相位2：90°
	AD9959_SetPSK(CH3,PSK_phase_ch3,1000);//设置PSK参数，CH3通道，P1=0°,P2=90°，频率1000hz
	
	//每个通道的PSK参数可以不同，单独设置即可。
	//也可不将所有通道都设置为PSK调制模式，则未设置PSK调制模式的通道，可使用AD9959_Set_Fre等函数独立设置其频率、相位、相位;或设置成其他调制模式。
	
	IO_Update();	//AD9959更新数据,调用此函数后，上述操作生效！！！！
	
	while(1)
	{
		
		PS0=0;PS1=0;PS2=0;PS3=0;//PS0-PS3分别控制CH0-CH3；低电平：输出相位1
		delay_ms(2);
		PS0=1;PS1=1;PS2=1;PS3=1;//高电平：输出相位2
		delay_ms(2);
	}
	

}


