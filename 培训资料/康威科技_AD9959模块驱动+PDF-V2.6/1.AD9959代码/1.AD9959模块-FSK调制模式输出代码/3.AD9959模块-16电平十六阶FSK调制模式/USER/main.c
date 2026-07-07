/**********************************************************
                       康威电子
功能：stm32f103rct6控制AD9959模块16电平十六阶FSK调制模式
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
	uint32_t FSK_fre_ch0[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//FSK数据
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
	AD9959_Modulation_Init(CH0,FSK,SWEEP_DISABLE,LEVEL_MOD_16);//通道0设置为FSK模式，不启用线性扫描，16电平调制
	
	FSK_fre_ch0[0]=10000;//FSK频率1：10000Hz
	FSK_fre_ch0[1]=20000;//FSK频率2：20000Hz
	FSK_fre_ch0[2]=50000;//FSK频率3：50000Hz
	FSK_fre_ch0[3]=80000;//FSK频率4：80000Hz
	FSK_fre_ch0[4]=120000;//FSK频率5：120000Hz
	FSK_fre_ch0[5]=140000;//FSK频率6：140000Hz
	FSK_fre_ch0[6]=180000;//FSK频率7：180000Hz
	FSK_fre_ch0[7]=200000;//FSK频率8：200000Hz
	
	FSK_fre_ch0[8]=250000;//FSK频率9：...
	FSK_fre_ch0[9]=270000;//FSK频率10：...
	FSK_fre_ch0[10]=300000;//FSK频率11：...
	FSK_fre_ch0[11]=320000;//FSK频率12：...
	FSK_fre_ch0[12]=350000;//FSK频率13：...
	FSK_fre_ch0[13]=400000;//FSK频率14：...
	FSK_fre_ch0[14]=600000;//FSK频率15：...
	FSK_fre_ch0[15]=10000000;//FSK频率16：...
	
	AD9959_SetFSK(CH0,FSK_fre_ch0,0);//设置FKS参数，CH0通道，F1=10000Hz,F2=20000Hz，F3=50000Hz........，相位0(0度)

	//也可不将所有通道都设置为FSK调制模式，则未设置FSK调制模式的通道，可使用AD9959_Set_Fre等函数独立设置其频率、幅度、相位;或设置成其他调制模式。
	
	IO_Update();	//AD9959更新数据,调用此函数后，上述操作生效！！！！
	
	while(1)
	{
		PS0=0;PS1=0;PS2=0;PS3=0;//PS0-PS3控制CH0；CH0输出频率1
		delay_us(15);
		
		PS0=0;PS1=0;PS2=0;PS3=1;//CH0输出频率2
		delay_us(15);
		
		PS0=0;PS1=0;PS2=1;PS3=0;//CH0输出频率3
		delay_us(15);
		
		PS0=0;PS1=0;PS2=1;PS3=1;//CH0输出频率4
		delay_us(15);		
		
		PS0=0;PS1=1;PS2=0;PS3=0;//CH0输出频率5
		delay_us(15);
		
		PS0=0;PS1=1;PS2=0;PS3=1;//CH0输出频率6
		delay_us(15);
	
		PS0=0;PS1=1;PS2=1;PS3=0;//CH0输出频率7
		delay_us(15);
		
		PS0=0;PS1=1;PS2=1;PS3=1;//CH0输出频率8
		delay_us(15);
		
		PS0=1;PS1=0;PS2=0;PS3=0;//CH0输出频率9
		delay_us(15);
		
		PS0=1;PS1=0;PS2=0;PS3=1;//CH0输出频率10
		delay_us(15);
		
		PS0=1;PS1=0;PS2=1;PS3=0;//CH0输出频率11
		delay_us(15);
		
		PS0=1;PS1=0;PS2=1;PS3=1;//CH0输出频率12
		delay_us(15);		
		
		PS0=1;PS1=1;PS2=0;PS3=0;//CH0输出频率13
		delay_us(15);
		
		PS0=1;PS1=1;PS2=0;PS3=1;//CH0输出频率14
		delay_us(15);
	
		PS0=1;PS1=1;PS2=1;PS3=0;//CH0输出频率15
		delay_us(15);
		
		PS0=1;PS1=1;PS2=1;PS3=1;//CH0输出频率16
		delay_us(15);
	}
	
}








