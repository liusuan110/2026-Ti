#ifndef H_APP_H
#define H_APP_H

/* H题 应用编排: 采集C -> 分析(频率/波形/相位) -> AD9959重建A'/B' -> PLL闭环锁相 */

void H_App_init(void);
void H_App_loop(void);

#endif /* H_APP_H */
