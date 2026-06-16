#ifndef MATHACL_UTIL_H
#define MATHACL_UTIL_H

#include <stdint.h>

/* MATHACL 硬件数学加速器封装 (M0+ 无 FPU, 用硬件做 atan2/sqrt).
 * 注意: 本工程 .syscfg 未配置 MATHACL, 需在 MathUtil_init() 手动复位+上电. */

void     MathUtil_init(void);                       /* reset + enablePower */
int32_t  MathUtil_atan2_q31(int32_t x, int32_t y);  /* 返回归一化角(满量程±pi) */
uint32_t MathUtil_sqrt_q31(uint32_t v);             /* Q31 开方 */

#endif /* MATHACL_UTIL_H */
