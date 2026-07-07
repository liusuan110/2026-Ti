/**********************************************************
                       康威电子
功能：stm32f103rct6控制AD9959模块4电平四阶FSK调制
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
	uint32_t FSK_fre_ch0[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	uint32_t FSK_fre_ch1[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//FSK数据
	MY_NVIC_PriorityGroup_Config(NVIC_PriorityGroup_2);	//设置中断分组
	delay_init(72);	//初始化延时函数
	delay_ms(500);//延时一会儿，等待上电稳定,确保AD9959比控制板先上电。
	
	//代码移植建议
	//1.修改头文件AD9959.h中，自己控制板实际需要使用哪些控制引脚。如CS脚改成PA0控制，则定义"#define CS			PAout(0)" 
	//2.修改C文件AD9959.c中，AD9959_Init函数，所有用到管脚的GPIO输出功能初始化
	//3.完成

	AD9959_Init();	//初始化控制AD9959需要用到的IO口,及寄存器
	
//	最多只能同时有2个通道，设置成4电平调制。
//	本函数只能选择CH0-1，未做任意通道兼容，具体方法请参考AD9959芯片手册22-23页，操作FR1[14:12]为对应值。
	AD9959_Modulation_Init(CH0|CH1,FSK,SWEEP_DISABLE,LEVEL_MOD_4);//通道0-1设置为FSK模式，不启用线性扫描，4电平调制
	
	FSK_fre_ch0[0]=100;//FSK频率1：100Hz
	FSK_fre_ch0[1]=1000;//FSK频率2：1000Hz
	FSK_fre_ch0[2]=10000;//FSK频率3：10000Hz
	FSK_fre_ch0[3]=100000;//FSK频率4：100000Hz
	AD9959_SetFSK(CH0,FSK_fre_ch0,0);//设置FKS参数，CH0通道，F1=100Hz,F2=1000Hz，F3=10000Hz,F4=100000Hz，相位0(0度)
	
	FSK_fre_ch1[0]=100;//FSK频率1：100Hz
	FSK_fre_ch1[1]=1000;//FSK频率2：1000Hz
	FSK_fre_ch1[2]=10000;//FSK频率3：10000Hz
	FSK_fre_ch1[3]=100000;//FSK频率4：100000Hz
	AD9959_SetFSK(CH1,FSK_fre_ch1,0);//设置FKS参数，CH1通道，F1=100Hz,F2=1000Hz，F3=10000Hz,F4=100000Hz，相位0(0度)
	
	//每个通道的FSK参数可以不同，单独设置即可。
	//也可不将所有通道都设置为FSK调制模式，则未设置FSK调制模式的通道，可使用AD9959_Set_Fre等函数独立设置其频率、幅度、相位;或设置成其他调制模式。
	
	IO_Update();	//AD9959更新数据,调用此函数后，上述操作生效！！！！
	
	while(1)
	{
		
		PS0=0;PS1=0;//PS0、PS1分别控制CH0；CH0输出频率1
		PS2=0;PS3=0;//PS2、PS3分别控制CH1；CH1输出频率1
		delay_ms(10);
		
		PS0=0;PS1=1;//CH0输出频率2
		PS2=0;PS3=1;//CH1输出频率2
		delay_ms(10);
		
		PS0=1;PS1=0;//CH0输出频率3
		PS2=1;PS3=0;//CH1输出频率3
		delay_ms(10);
		
		PS0=1;PS1=1;//CH0输出频率4
		PS2=1;PS3=1;//CH1输出频率4
		delay_ms(10);
	}
	
}


