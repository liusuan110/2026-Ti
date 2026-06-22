#ifndef TELEMETRY_CORE_H
#define TELEMETRY_CORE_H

#include <stdint.h>
#include "h_config.h"

/* ============================================================================
 *  上板可观测性 —— 遥测"格式化核"(纯 C, 无硬件/库依赖, 可离板单元测试)
 *
 *  目的: 上板 bring-up 时, 把每个控制周期的关键诊断量(各通道 ADC 原始统计、
 *        测得频率/波形/相位、PLL 误差/FTW/锁定)拼成一行 ASCII 文本, 由
 *        telemetry.c 经 UART 吐出. 用串口助手看 / 存 CSV 画图 —— 取代"挂 JTAG
 *        翻内存窗口"的盲调.
 *
 *  本文件只做"数 -> 字符串", 不碰 UART; 整数转十进制/定点全部自己实现
 *  (M0+ 无 FPU, 不用 printf/%f, 省 Flash 也避免浮点格式化). 见 host_test/test_telemetry.c.
 * ========================================================================== */

/* 一帧诊断快照: h_app 每周期填好后交给格式化核. 全是普通数, 不依赖硬件类型. */
typedef struct {
    uint32_t frame;                      /* 帧计数 */
    uint8_t  stage;                      /* 当前 bring-up 阶段 1..4 */
    uint8_t  built;                      /* 是否已建链(进入闭环) */
    /* 各路 ADC 原始统计(12 位码 0..4095): 看信号在不在/有没有削顶/偏置对不对 */
    uint16_t c_min,  c_mean,  c_max;     /* 混合信号 C (ADC0) */
    uint16_t fb_min, fb_mean, fb_max;    /* 反馈 A'+B' (ADC1) */
    /* 分析结果(每路) */
    uint32_t freq[H_NUM_SIGNALS];        /* 测得频率 Hz */
    int32_t  phase_q31[H_NUM_SIGNALS];   /* 原信号相位(Q31 归一化角, ±2^31=±180°) */
    uint16_t amp[H_NUM_SIGNALS];         /* FFT 幅值 */
    uint16_t bin[H_NUM_SIGNALS];         /* 峰值 bin */
    uint8_t  wave[H_NUM_SIGNALS];        /* 0=正弦 1=三角 (h_wave_t) */
    uint8_t  valid[H_NUM_SIGNALS];
    uint8_t  overlap[H_NUM_SIGNALS];
    /* PLL 诊断(每路; 仅闭环阶段有效) */
    int32_t  perr_q31[H_NUM_SIGNALS];    /* 相位误差(Q31 归一化角) */
    uint32_t ftw[H_NUM_SIGNALS];         /* 当前 FTW */
    uint8_t  locked[H_NUM_SIGNALS];
} tel_frame_t;

/* 缓冲统计: 求 min/mean/max(12 位 ADC 码). n=0 时全置 0. 纯函数. */
void TelCore_bufStats(const uint16_t *buf, uint16_t n,
                      uint16_t *out_min, uint16_t *out_mean, uint16_t *out_max);

/* 无符号转十进制 ASCII, 写入 dst(不加 '\0'), 返回写入字符数. */
uint8_t TelCore_u32(char *dst, uint32_t v);

/* 有符号转十进制 ASCII(带正负号), 返回字符数. */
uint8_t TelCore_i32(char *dst, int32_t v);

/* Q31 归一化角(±2^31=±180°) -> 十分之一度(整数, 带符号). 例: +123 表示 +12.3°. */
int32_t TelCore_q31ToDeci(int32_t q31);

/* 把"十分之一度"格式化成 "+12.3" 形式, 返回字符数. */
uint8_t TelCore_deci(char *dst, int32_t deci);

/* 组装一整行诊断文本(以 '\n' 结尾, 加 '\0'). cap 为 dst 容量(建议 >= 256).
 * 返回不含 '\0' 的字符数(若超出 cap 会安全截断). 纯函数, 可离板验证. */
uint16_t TelCore_formatLine(char *dst, uint16_t cap, const tel_frame_t *f);

#endif /* TELEMETRY_CORE_H */
