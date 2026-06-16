#include "reconstruct.h"
#include "AD9959.h"
#include "DDS.h"

/* idx 0/1 -> AD9959 通道使能位 CH0/CH1 */
static uint8_t ch_of(uint8_t idx)
{
    return (idx == 0u) ? (uint8_t)AD9959_CH0 : (uint8_t)AD9959_CH1;
}

void Reconstruct_init(void)
{
    DDS_init();   /* 初始化 AD9959 (内部含 AD9959_init) */
}

void Reconstruct_set(uint8_t idx, const h_sig_t *sig)
{
    uint8_t ch = ch_of(idx);

    AD9959_setFreq(ch, (float)sig->freq_hz);            /* 频率->FTW(驱动按500MHz) */
    AD9959_setAmp(ch, 1023u);                           /* 满幅, 后级运放调到 >=1Vpp */
    AD9959_setPhase(ch, Recon_angleQ31ToCpow(sig->phase_q31)); /* 归一化角->14位CPOW(已修正 >>18) */

    /* TODO 三角波: AD9959 出正弦, 三角=比较器整型+积分器(硬件路径) */
    (void)sig->wave;

    AD9959_IOUpdate();
}

void Reconstruct_setPhase14(uint8_t idx, uint16_t phase14)
{
    AD9959_setPhase(ch_of(idx), (uint16_t)(phase14 & 0x3FFFu));
    AD9959_IOUpdate();
}

void Reconstruct_setFreqHz(uint8_t idx, float freq_hz)
{
    AD9959_setFreq(ch_of(idx), freq_hz);
    AD9959_IOUpdate();
}

void Reconstruct_setPhaseDiffDeg(uint8_t idx, uint16_t ref_cpow, int32_t deg)
{
    uint16_t p = (uint16_t)(((uint32_t)ref_cpow + Recon_degToCpow(deg)) & 0x3FFFu);
    AD9959_setPhase(ch_of(idx), p);
    AD9959_IOUpdate();
}
