#include "pll.h"

void Pll_init(pll_t *p, int32_t kp, int32_t ki, int32_t integ_max)
{
    p->kp        = kp;
    p->ki        = ki;
    p->integ     = 0;
    p->integ_max = integ_max;
    p->out       = 0;
}

int32_t Pll_update(pll_t *p, int32_t phase_err)
{
    /* 积分 + 抗饱和 */
    p->integ += phase_err;
    if (p->integ >  p->integ_max) { p->integ =  p->integ_max; }
    if (p->integ < -p->integ_max) { p->integ = -p->integ_max; }

    /* 定点 PI: out = (kp*err + ki*integ) >> 8  (缩放系数待上板标定) */
    p->out = ((p->kp * phase_err) + (p->ki * p->integ)) >> 8;
    return p->out;
}
