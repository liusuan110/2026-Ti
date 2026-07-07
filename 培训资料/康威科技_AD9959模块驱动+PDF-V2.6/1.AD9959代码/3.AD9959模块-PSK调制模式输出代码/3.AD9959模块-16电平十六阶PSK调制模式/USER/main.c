/**********************************************************
                       康威电子
功能：stm32f103rct6控制AD9959模块16电平十六阶PSK调制模式
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
	uint16_t PSK_phase_ch0[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//PSK数据
	MY_NVIC_PriorityGroup_Config(NVIC_PriorityGroup_2);	//设置中断分组
	delay_init(72);	//初始化延时函数
	delay_ms(500);//延时一会儿，等待上电稳定,确保AD9959比控制板先上电。
	
	//代码移植建议
	//1.修改头文件AD9959.h中，自己控制板实际需要使用哪些控制引脚。如CS脚改成PA0控制，则定义"#define CS			PAout(0)" 
	//2.修改C文件AD9959.c中，AD9959_Init函数，所有用到管脚的GPIO输出功能初始化
	//3.完成

	AD9959_Init();	//初始化控制AD9959需要用到的IO口,及寄存器
	
//	最多只能同时有1个通道，设置成16电平调制。
//	本函数只能选择CH0做16电平调制，未做任意通道兼容，具体方法请参考AD9959芯片手册22-23页，操作FR1[14:12]为对应值。
	AD9959_Modulation_Init(CH0,PSK,SWEEP_DISABLE,LEVEL_MOD_16);//通道0设置为PSK模式，不启用线性扫描，16电平调制
	
	PSK_phase_ch0[0]=0;//PSK相位1：0，范围：0~16383(对应角度：0°~360°)
	PSK_phase_ch0[1]=1024;//PSK相位2：22.5°
	PSK_phase_ch0[2]=2048;//PSK相位3：45°
	PSK_phase_ch0[3]=3072;//PSK相位4：67.5°
	PSK_phase_ch0[4]=4096;//PSK相位5：90°
	PSK_phase_ch0[5]=5120;//PSK相位6：...
	PSK_phase_ch0[6]=6144;//PSK相位7：...
	PSK_phase_ch0[7]=7168;//PSK相位8：...
	
	PSK_phase_ch0[8]=8192;//PSK相位9：...
	PSK_phase_ch0[9]=9216;//PSK相位10：...
	PSK_phase_ch0[10]=10240;//PSK相位11：...
	PSK_phase_ch0[11]=11264;//PSK相位12：...
	PSK_phase_ch0[12]=12288;//PSK相位13：...
	PSK_phase_ch0[13]=13312;//PSK相位14：...
	PSK_phase_ch0[14]=14336;//PSK相位15：...
	PSK_phase_ch0[15]=15360;//PSK相位16：337.5°
	
	AD9959_SetPSK(CH0,PSK_phase_ch0,1000);//设置PSK参数，CH1通道，写入相位，频率1000hz
	
	//也可不将所有通道都设置为PSK调制模式，则未设置PSK调制模式的通道，可使用AD9959_Set_Fre等函数独立设置其频率、幅度、相位;或设置成其他调制模式。
	
	IO_Update();	//AD9959更新数据,调用此函数后，上述操作生效！！！！
	
	while(1)
	{
		PS0=0;PS1=0;PS2=0;PS3=0;//PS0-PS3控制CH0；CH0输出相位1
		delay_ms(2);
		
		PS0=0;PS1=0;PS2=0;PS3=1;//CH0输出相位2
		delay_ms(2);
		
		PS0=0;PS1=0;PS2=1;PS3=0;//CH0输出相位3
		delay_ms(2);
		
		PS0=0;PS1=0;PS2=1;PS3=1;//CH0输出相位4
		delay_ms(2);		
		
		PS0=0;PS1=1;PS2=0;PS3=0;//CH0输出相位5
		delay_ms(2);
		
		PS0=0;PS1=1;PS2=0;PS3=1;//CH0输出相位6
		delay_ms(2);
	
		PS0=0;PS1=1;PS2=1;PS3=0;//CH0输出相位7
		delay_ms(2);
		
		PS0=0;PS1=1;PS2=1;PS3=1;//CH0输出相位8
		delay_ms(2);
		
		PS0=1;PS1=0;PS2=0;PS3=0;//CH0输出相位9
		delay_ms(2);
		
		PS0=1;PS1=0;PS2=0;PS3=1;//CH0输出相位10
		delay_ms(2);
		
		PS0=1;PS1=0;PS2=1;PS3=0;//CH0输出相位11
		delay_ms(2);
		
		PS0=1;PS1=0;PS2=1;PS3=1;//CH0输出相位12
		delay_ms(2);		
		
		PS0=1;PS1=1;PS2=0;PS3=0;//CH0输出相位13
		delay_ms(2);
		
		PS0=1;PS1=1;PS2=0;PS3=1;//CH0输出相位14
		delay_ms(2);
	
		PS0=1;PS1=1;PS2=1;PS3=0;//CH0输出相位15
		delay_ms(2);
		
		PS0=1;PS1=1;PS2=1;PS3=1;//CH0输出相位16
		delay_ms(2);
	}
	
}








