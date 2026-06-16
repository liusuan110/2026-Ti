#include "h_app.h"
#include "h_config.h"
#include "sig_acq.h"
#include "sig_analysis.h"
#include "reconstruct.h"
#include "pll.h"
#include "mathacl_util.h"
#include "Tick.h"

#define H_CAP_N   ((uint16_t)H_FFT_N)

/* 大缓冲全局, 不放栈; 4 字节对齐(ADC DMA 按 32 位字写入) */
static uint16_t   s_buf[H_CAP_N] __attribute__((aligned(4)));
static h_result_t s_res;
static pll_t      s_pll[H_NUM_SIGNALS];
static uint8_t    s_built = 0U;

void H_App_init(void)
{
    MathUtil_init();        /* MATHACL 上电(.syscfg 未配, 手动) */
    SigAcq_init();          /* ADC0 + FIFO + DMA */
    SigAnalysis_init();
    Reconstruct_init();     /* AD9959 (DDS_init) */

    /* PI 参数初值(待标定); 整体框架先跑通 */
    Pll_init(&s_pll[0], 64, 8, (1 << 20));
    Pll_init(&s_pll[1], 64, 8, (1 << 20));
    s_built = 0U;
}

void H_App_loop(void)
{
    uint8_t i;

    /* 1) 采集混合信号 C */
    (void)SigAcq_capture(s_buf, H_CAP_N);

    /* 2) 分析: 两路 频率/幅度/相位/波形 */
    (void)SigAnalysis_run(s_buf, H_CAP_N, &s_res);

    if (s_built == 0U) {
        /* 3) 首次: 按测得参数重建 A'/B' */
        for (i = 0U; i < (uint8_t)H_NUM_SIGNALS; i++) {
            Reconstruct_set(i, &s_res.sig[i]);
        }
        s_built = 1U;
    } else {
        /* 4) 闭环 PLL: 用相位误差微调相位, 锁相抗漂移 (骨架)
         *    TODO: err 应为 A'(重建) 与 A(原信号) 的相位差;
         *          稳态后再切到调 FTW 的带预调谐 PLL */
        for (i = 0U; i < (uint8_t)H_NUM_SIGNALS; i++) {
            int32_t err = s_res.sig[i].phase_q31;
            int32_t adj = Pll_update(&s_pll[i], err);
            uint16_t ph = (uint16_t)((uint32_t)(adj >> 17) & 0x3FFFU);
            Reconstruct_setPhase14(i, ph);
        }
    }

    /* 控制周期固定(培训3 PPT 要点: 反馈环路控制间隔需为固定值) */
    Tick_delay(50U);
}
