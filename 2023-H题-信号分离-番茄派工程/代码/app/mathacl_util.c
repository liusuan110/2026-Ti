#include "mathacl_util.h"
#include "ti_msp_dl_config.h"          /* 拉入 driverlib + 器件头 (MATHACL, __MSPM0_HAS_MATHACL__) */
#include <ti/driverlib/dl_mathacl.h>

/* ARCTAN2: op1=X, op2=Y -> RES1=归一化角, RES2=模 (signed 才能四象限) */
static const DL_MathACL_operationConfig kAtan2Cfg = {
    .opType      = DL_MATHACL_OP_TYPE_ARCTAN2,
    .opSign      = DL_MATHACL_OPSIGN_SIGNED,
    .iterations  = 10U,
    .scaleFactor = 0U,
    .qType       = DL_MATHACL_Q_TYPE_Q31,
};

/* SQRT: op1=value, 需要 scaleFactor>0 -> RES1=sqrt */
static const DL_MathACL_operationConfig kSqrtCfg = {
    .opType      = DL_MATHACL_OP_TYPE_SQRT,
    .opSign      = DL_MATHACL_OPSIGN_UNSIGNED,
    .iterations  = 10U,
    .scaleFactor = 1U,
    .qType       = DL_MATHACL_Q_TYPE_Q31,
};

void MathUtil_init(void)
{
    /* .syscfg 未配 MATHACL, 手动复位+上电 (侦察确认: 否则寄存器读回垃圾) */
    DL_MathACL_reset(MATHACL);
    DL_MathACL_enablePower(MATHACL);
}

int32_t MathUtil_atan2_q31(int32_t x, int32_t y)
{
    DL_MathACL_startArcTan2Operation(MATHACL, &kAtan2Cfg, (uint32_t)x, (uint32_t)y);
    DL_MathACL_waitForOperation(MATHACL);
    return (int32_t)DL_MathACL_getResultOne(MATHACL);   /* 归一化角 */
}

uint32_t MathUtil_sqrt_q31(uint32_t v)
{
    DL_MathACL_startSqrtOperation(MATHACL, &kSqrtCfg, v);
    DL_MathACL_waitForOperation(MATHACL);
    return DL_MathACL_getResultOne(MATHACL);
}
