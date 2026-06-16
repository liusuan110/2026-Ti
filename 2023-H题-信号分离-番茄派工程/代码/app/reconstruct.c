#include "reconstruct.h"
#include "AD9959.h"
#include "DDS.h"

/* idx 0/1 -> AD9959 通道使能位 CH0/CH1 */
static uint8_t ch_of(uint8_t idx)
{
    return (idx == 0U) ? (uint8_t)AD9959_CH0 : (uint8_t)AD9959_CH1;
}

void Reconstruct_init(void)
{
    DDS_init();   /* 初始化 AD9959 (内部含 AD9959_init) */
}

void Reconstruct_set(uint8_t idx, const h_sig_t *sig)
{
    uint8_t  ch = ch_of(idx);
    uint16_t ph14;

    AD9959_setFreq(ch, (float)sig->freq_hz);
    AD9959_setAmp(ch, 1023U);                 /* 满幅, 后级运放调到 >=1Vpp */

    /* 归一化角(±pi) -> 14 位相位字(0..0x3FFF). 缩放待上板标定 */
    ph14 = (uint16_t)((((int64_t)sig->phase_q31) * 0x4000) >> 31) & 0x3FFFU;
    AD9959_setPhase(ch, ph14);

    /* TODO 三角波: AD9959 出正弦, 三角=比较器整型+积分器(硬件路径) */
    (void)sig->wave;

    AD9959_IOUpdate();
}

void Reconstruct_setPhase14(uint8_t idx, uint16_t phase14)
{
    AD9959_setPhase(ch_of(idx), (uint16_t)(phase14 & 0x3FFFU));
    AD9959_IOUpdate();
}

void Reconstruct_setFreqHz(uint8_t idx, float freq_hz)
{
    AD9959_setFreq(ch_of(idx), freq_hz);
    AD9959_IOUpdate();
}
