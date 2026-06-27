#ifndef H_CONFIG_H
#define H_CONFIG_H

#include <stdint.h>

/* ===================== 2023 H题 信号分离 — 全局参数 =====================
 * 平台: 番茄派 MSPM0G3507 (Cortex-M0+ @80MHz, 双12位4MSPS ADC, MATHACL, 无FPU,
 *       128KB Flash / 32KB SRAM)
 * 方案(再生法): MSPM0 做 FFT 分析(频率/波形/相位) + AD9959 重建 A'/B' + 软件PLL锁相
 * 参数依据: 2026 北京电设培训3(周润华) —— fs=1.024MSa/s, N=1024, fRes=1kHz
 * ====================================================================== */

/* 系统时钟: 片内 SYSOSC 32MHz ×5 /2 = 80MHz (SYSPLL, 见 syscfg; BUILD-3 已去外部时钟依赖) */
#define H_SYS_CLK_HZ        80000000UL

/* ===================== 上板 bring-up 开关(联调用, 见 上板联调清单.md) =====================
 * 这些开关让"没板子时写好的固件"在板子到手后能逐级、可观测地调通. 默认值即推荐量产值.
 * 均用 #ifndef 包裹 -> 可在编译选项里 -DH_STAGE=2 等覆盖, 不必改源码. */

/* 串口遥测: 1=经 UART 打印诊断行(bring-up 强烈建议开), 0=关闭(零运行开销).
 * 取代"挂 JTAG 翻内存"的盲调. TX 引脚/波特率见 syscfg(默认 PA10, 115200 8N1). */
#ifndef H_TELEMETRY
#define H_TELEMETRY         1
#endif

/* 遥测抽稀: 每 H_TEL_DECIM 个控制周期才打印一行. 原因: 115200 baud 阻塞发 ~150 字节≈13ms,
 * 每周期都发会撑爆 5ms 控制周期、破坏 PLL 固定 dt. 默认 8(≈每 40ms 一行, 够看).
 * 闭环(阶段4)做精细 dt 整定/计时时, 可设大些或临时 H_TELEMETRY=0; 想每周期看可在 syscfg 提高波特率. */
#ifndef H_TEL_DECIM
#define H_TEL_DECIM         8u
#endif

/* 分阶段 bring-up(每级是上一级的超集, 上板按 1->2->3->4 推进, 演示/量产用 4):
 *   1 只采集 C(+反馈) + 打印 ADC 原始统计      —— 验采集/引脚/削顶/偏置(必修2/4)
 *   2 +分析 C(频率/波形/相位/bin)              —— 验 FFT/测频/判波(先不接 DDS)
 *   3 +开环重建(建链一次, 不锁相)              —— 验 AD9959 出 A'/B'(必修3/建议5)
 *   4 +闭环 PLL(完整功能)                      —— 接反馈通道后锁相(必修1/建议1/2) */
#ifndef H_STAGE
#define H_STAGE             4
#endif

/* 采样触发源: 1=TIMA 周期事件【同时】触发 ADC0/ADC1(零时间差 + fs 稳定, 见 sig_acq.c/P1);
 *            0=软件触发(原路径, 作回退). 发挥②相位差≤5° 对零时间差敏感, 建议用 1. */
#ifndef H_ACQ_USE_TIMER_TRIGGER
#define H_ACQ_USE_TIMER_TRIGGER  1
#endif

/* 频率吸附: 1=把 FFT 插值频率吸附到最近 H_FREQ_STEP_HZ 网格(题目所有输入均在网格上),
 * 中和 fs 的 ±2% 绝对误差 -> AD9959 输出精确网格频率、PLL 只需拉两晶振 ppm 级差.
 * 0=用原始插值频率(仍可工作, 但 PLL 拉入范围更大). 实现见 sig_analysis_core.c. */
#ifndef H_FREQ_SNAP_GRID
#define H_FREQ_SNAP_GRID    1
#endif

/* 淘晶驰串口屏(TJC4827X243_011C)显示: 1=经 UART1 推送结果到屏, 0=关闭(零开销).
 * 输出两路 频率/波形/幅度 + 相位差 + 锁定; 只发变化字段(省带宽). UART1 引脚/波特率见 syscfg.
 * ★上位机要按 tjc_hmi_core.h 的控件名表在 page 0 建好 n0~n4/t0~t2. 与遥测(UART0)互不影响. */
#ifndef H_TJC_DISPLAY
#define H_TJC_DISPLAY       1
#endif

/* 屏幕刷新抽稀: 每 H_TJC_DECIM 个控制周期刷一次(只发变化字段). 默认 40(5ms周期≈5Hz, 够人眼看). */
#ifndef H_TJC_DECIM
#define H_TJC_DECIM         40u
#endif

/* ---- 发挥: 实时波形 / 触摸调相位 / 多页(见 tjc_hmi_core 协议注释 + 上板联调清单) ----
 * ★屏波特率须升到 115200 才够波形流畅(9600 下即便 addt 也仅~4Hz): syscfg UART_TJC.targetBaudRate
 *   + 上位机工程波特率 + 本固件三方一致. 详见 上板联调清单「串口屏波形/触摸」节. */

/* 波形页 addt 推流: 1=在波形页(page1)把混合信号 C 推到波形控件 s0, 0=不发波形. */
#ifndef H_TJC_WAVE
#define H_TJC_WAVE          1
#endif

/* 波形点数: 须 <= 屏波形控件 w 宽度(上位机实测) 且 <=1024(单次 addt 上限). 1024/200 整除. */
#ifndef H_TJC_WAVE_N
#define H_TJC_WAVE_N        200u
#endif

/* 波形帧抽稀: 每 N 个控制周期推一帧波形. 默认 40(115200 下一帧≈17ms阻塞, 5Hz; 与控制周期解耦). */
#ifndef H_TJC_WAVE_DECIM
#define H_TJC_WAVE_DECIM    40u
#endif

/* 波形发送法: 1=addt 透传(快, 推荐); 0=add 逐点(慢但确定, addt 上板验不通时回退, 建议同时调小 _N). */
#ifndef H_TJC_WAVE_USE_ADDT
#define H_TJC_WAVE_USE_ADDT 1
#endif

/* addt 就绪等待超时(ms): 等屏回 0xFE 就绪帧的上限, 超时放弃本帧(屏未进透传态, 安全). */
#ifndef H_TJC_WAVE_TIMEOUT_MS
#define H_TJC_WAVE_TIMEOUT_MS  50u
#endif

/* 触摸接收: 1=开 UART1 RX 中断 + 解析(触摸/相位/当前页/addt握手), 0=纯发送(零接收开销).
 * 注: H_TJC_WAVE 用 addt 也依赖 RX 收就绪帧, 故波形开时 RX 会被自动使能. */
#ifndef H_TJC_TOUCH
#define H_TJC_TOUCH         1
#endif

/* ===================== 锁相方式(同频稳定显示的两条路线, 二选一) =====================
 * 数字PLL(替代/备份, 已离板验证): 第2路ADC采A'+B' -> FFT -> atan2提相位 -> II型数字PLL调FTW.
 * 模拟鉴相+PID(主线/默认, 学长思路): 外部模拟乘法器(原信号×AD9959正交本振)-> 直流电压(=相位误差),
 *   ADC1读该电压 -> float PID 调 DDS 频率把相位拉到正交锁定. 固件更简单, 复杂度挪到模拟硬件.
 * 上板可两条都烧、A/B 对比; 模拟路调不通可退回数字PLL. */
#define H_LOCK_DIGITAL_PLL  0
#define H_LOCK_ANALOG_PID   1
#ifndef H_LOCK_METHOD
#define H_LOCK_METHOD       H_LOCK_ANALOG_PID
#endif

#if H_LOCK_METHOD == H_LOCK_ANALOG_PID
/* --- 模拟鉴相+PID 参数(全部上板可标定) --- */
#define H_PD_OFFSET_V       1.65f      /* 鉴相器零相位(正交)静态输出电压(V), 上板用示波器标定 */
#define H_PID_KP_50K        2.0f       /* 50kHz 处的基准比例增益(实际增益按频率调度) */
#define H_PID_KI_50K        1.0f       /* 50kHz 处的基准积分增益 */
#define H_PID_KD_50K        0.0f       /* 微分(默认关) */
#define H_PID_GAIN_PER_HZ   (1.0f/50000.0f) /* 增益调度因子: 实际增益=基准×(freq×此因子), 归一到50kHz */
#define H_PID_INT_MAX       2000.0f    /* 积分累加双向限幅(抗饱和) */
#define H_PID_OUT_MAX       50.0f      /* PID 输出(频率微调)双向限幅(Hz); 够覆盖两晶振 ppm 频差 */
#define H_PID_LOCK_VOLT     0.05f      /* |鉴相电压-偏置|<此值视为该路锁定(V), 上板标定 */
#define H_PID_LOCK_FRAMES   8u         /* 连续多帧达标才置锁定 */
#define H_QUAD_PHASE14      4096u      /* 正交本振相位 90°(AD9959 14位相位字: 16384=360°) */
#endif

/* 采样率 fs: 要分辨到 100kHz 三角波的 5 次谐波(500kHz) -> fs>1MSa/s; N 取 2 的幂.
 *  - 定时器触发(H_ACQ_USE_TIMER_TRIGGER=1): fs 精确 = 定时器BUSCLK / (LOAD+1).
 *    syscfg 的 TIMER_ADC 周期 977ns @80MHz -> LOAD=77 -> fs = 80e6/78 = 1025641 Hz(稳定, 无抖).
 *    ★改 syscfg 定时器周期时, 须同步本值(= 80000000/(TIMER_ADC_INST_LOAD_VALUE+1)).
 *  - 软件触发(=0): fs 为采样窗估算值, 须上板标定(必修4)写回本值. */
#if H_ACQ_USE_TIMER_TRIGGER
#define H_ADC_FS_HZ         1025641UL    /* = 80MHz / 78, 由 TIMER_ADC LOAD=77 精确确定 */
#else
#define H_ADC_FS_HZ         1024000UL    /* 软件触发: 估算, 须标定(必修4) */
#endif
#define H_FFT_N             1024U
#define H_FREQ_RES_HZ       (H_ADC_FS_HZ / H_FFT_N)   /* 定时器模式 ≈1001.6 Hz */

/* 候选/分析频率范围: 10~100kHz, 5kHz 步进 */
#define H_FREQ_MIN_HZ       10000U
#define H_FREQ_MAX_HZ       100000U
#define H_FREQ_STEP_HZ      5000U
#define H_FREQ_CAND_N       19U      /* (100-10)/5 + 1 */

/* 两路被分离信号 */
#define H_NUM_SIGNALS       2U

/* 波形类型 */
typedef enum { H_WAVE_SINE = 0, H_WAVE_TRIANGLE = 1 } h_wave_t;

/* 单路信号分析结果 */
typedef struct {
    uint32_t freq_hz;     /* 频率 (Hz) */
    uint16_t amp;         /* 幅度 (FFT 幅值, 相对值; 谐波重叠时为去污后的值) */
    int32_t  phase_q31;   /* 相位 (MATHACL 归一化角, 满量程对应 ±pi) */
    uint16_t bin;         /* 峰值所在 FFT bin (ANA-1: 相位取此 bin, 不由频率反算) */
    h_wave_t wave;        /* 波形类型 */
    uint8_t  valid;       /* 是否有效 */
    uint8_t  overlap;     /* fB≈3fA 谐波重叠标志(此路基波受另一路谐波污染, 已去污估计) */
} h_sig_t;

/* 整体分析结果: 两路, 按频率升序 [0]=低频A, [1]=高频B */
typedef struct {
    h_sig_t sig[H_NUM_SIGNALS];
} h_result_t;

#endif /* H_CONFIG_H */
