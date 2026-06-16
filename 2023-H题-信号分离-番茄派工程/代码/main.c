/*
 * 2023 H题 信号分离装置 —— 番茄派 MSPM0G3507 主程序
 * 方案: 再生法 (MSPM0 FFT分析 + AD9959 重建 + 软件PLL锁相)
 * 注意: AD9959 模块须先于单片机上电, 否则其 25MHz->500MHz PLL 时钟错误.
 */
#include "ti_msp_dl_config.h"
#include "h_app.h"

int main(void)
{
    SYSCFG_DL_init();   /* SysConfig: 80MHz 时钟 + GPIO_DDS + SysTick(1ms) */
    H_App_init();       /* MATHACL/ADC/AD9959 初始化 */

    while (1) {
        H_App_loop();   /* 采集 -> 分析 -> 重建/锁相 */
    }
}
