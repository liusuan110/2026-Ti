/**********************************************************
                       康威电子
功能：stm32f103rct6控制AD9959模块8电平八阶ASK调制
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
	uint16_t ASK_amp_ch0[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//ASK数据
	MY_NVIC_PriorityGroup_Config(NVIC_PriorityGroup_2);	//设置中断分组
	delay_init(72);	//初始化延时函数
	delay_ms(500);//延时一会儿，等待上电稳定,确保AD9959比控制板先上电。
	
	//代码移植建议
	//1.修改头文件AD9959.h中，自己控制板实际需要使用哪些控制引脚。如CS脚改成PA0控制，则定义"#define CS			PAout(0)" 
	//2.修改C文件AD9959.c中，AD9959_Init函数，所有用到管脚的GPIO输出功能初始化
	//3.完成

	AD9959_Init();	//初始化控制AD9959需要用到的IO口,及寄存器
	
//	最多只能同时有1个通道，设置成8电平调制。
//	本函数只能选择CH0做8电平调制，未做任意通道兼容，具体方法请参考AD9959芯片手册22-23页，操作FR1[14:12]为对应值。
	AD9959_Modulation_Init(CH0,ASK,SWEEP_DISABLE,LEVEL_MOD_8);//通道0设置为ASK模式，不启用线性扫描，8电平调制
	
	ASK_amp_ch0[0]=127;//ASK幅度1：八分之一 范围（0-1023）
	ASK_amp_ch0[1]=255;//ASK幅度2：八分之二
	ASK_amp_ch0[2]=383;//ASK幅度3：..
	ASK_amp_ch0[3]=511;//ASK幅度4：..
	ASK_amp_ch0[4]=639;//ASK幅度5：..
	ASK_amp_ch0[5]=767;//ASK幅度6：..
	ASK_amp_ch0[6]=895;//ASK幅度7：..
	ASK_amp_ch0[7]=1023;//ASK幅度8：最大
	AD9959_SetASK(CH0,ASK_amp_ch0,1000,0);//设置ASK参数，CH0通道，写入电压幅度，频率1000hz，相位0(0度)

	//每个通道的ASK参数可以不同，单独设置即可。
	//也可不将所有通道都设置为ASK调制模式，则未设置ASK调制模式的通道，可使用AD9959_Set_Fre等函数独立设置其频率、幅度、相位;或设置成其他调制模式。
	
	IO_Update();	//AD9959更新数据,调用此函数后，上述操作生效！！！！
	
	while(1)
	{
		
		PS0=0;PS1=0;PS2=0;//PS0-PS2控制CH0；CH0输出幅度1
		delay_ms(10);
		
		PS0=0;PS1=0;PS2=1;//CH0输出幅度2
		delay_ms(10);
		
		PS0=0;PS1=1;PS2=0;//CH0输出幅度3
		delay_ms(10);
		
		PS0=0;PS1=1;PS2=1;//CH0输出幅度4
		delay_ms(10);		
		
		PS0=1;PS1=0;PS2=0;//CH0输出幅度5
		delay_ms(10);
		
		PS0=1;PS1=0;PS2=1;//CH0输出幅度6
		delay_ms(10);
	
		PS0=1;PS1=1;PS2=0;//CH0输出幅度7
		delay_ms(10);
		
		PS0=1;PS1=1;PS2=1;//CH0输出幅度8
		delay_ms(10);
	}
	
}








