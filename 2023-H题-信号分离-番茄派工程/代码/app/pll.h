#ifndef PLL_H
#define PLL_H

#include <stdint.h>

/* 软件锁相环: 把 AD9959 输出锁到参考信号 C, 抵抗"信号源与DDS独立晶振"导致的漂移.
 * 带预调谐: FTW = 预设值(目标频率) + 环路输出; PI 参数随频率缩放(培训3 PPT 要点).
 * 这里只实现通用 PI 环路, FTW/相位的具体施加在 reconstruct/h_app 中完成. */

typedef struct {
    int32_t kp;          /* 比例系数(定点) */
    int32_t ki;          /* 积分系数(定点) */
    int32_t integ;       /* 积分累加值 */
    int32_t integ_max;   /* 抗积分饱和 */
    int32_t out;         /* 环路输出 */
} pll_t;

void    Pll_init(pll_t *p, int32_t kp, int32_t ki, int32_t integ_max);
int32_t Pll_update(pll_t *p, int32_t phase_err);   /* 输入相位误差, 输出修正量 */

#endif /* PLL_H */
