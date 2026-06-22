#ifndef TJC_HMI_CORE_H
#define TJC_HMI_CORE_H

#include <stdint.h>
#include "h_config.h"

/* ============================================================================
 *  淘晶驰串口屏 (USART HMI) 指令构建"核"(纯 C, 无硬件依赖, 可离板单元测试)
 *  目标屏: TJC4827X243_011C (X2系列, 4.3", 480x272, 电容触摸)
 *
 *  协议(见 http://wiki.tjc1688.com/commands/): 指令为 ASCII 文本, 以【三个 0xFF】结尾.
 *    赋值文本:  t0.txt="SINE"  + FF FF FF
 *    赋值数字:  n0.val=50000   + FF FF FF
 *    其它指令:  page 0 / dim=80 + FF FF FF
 *
 *  本核只做"数 -> 指令字节", 不碰 UART(由 tjc_hmi.c 发送). 数字转 ASCII 复用 telemetry_core.
 *  关键: buildView 只发【变化】的字段(与 last 比较), 锁定稳定后几乎不发 -> 把刷屏对
 *        控制周期的扰动降到最低. 见 host_test/test_tjc_hmi.c.
 *
 *  ★上位机(USART HMI 编辑器)需在【主页 page 0】建好下列控件, 对象名严格一致:
 *    | 对象 | 类型   | 含义                    |
 *    | n0   | number | A′(低频) 频率 Hz        |
 *    | n1   | number | A′ 幅度(相对)           |
 *    | t0   | text   | A′ 波形 "SINE"/"TRI"    |
 *    | n2   | number | B′(高频) 频率 Hz        |
 *    | n3   | number | B′ 幅度(相对)           |
 *    | t1   | text   | B′ 波形                 |
 *    | n4   | number | 相位差 B′−A′ (0..180°)  |
 *    | t2   | text   | 锁定 "LOCK"/"UNLK"      |
 *    其余静态文字(单位 Hz/°、标题、表头)在编辑器里画, 不经串口发. 改名则改下面的宏.
 * ========================================================================== */

#define TJC_OBJ_FREQ_A   "n0"
#define TJC_OBJ_AMP_A    "n1"
#define TJC_OBJ_WAVE_A   "t0"
#define TJC_OBJ_FREQ_B   "n2"
#define TJC_OBJ_AMP_B    "n3"
#define TJC_OBJ_WAVE_B   "t1"
#define TJC_OBJ_PHASE    "n4"
#define TJC_OBJ_LOCK     "t2"

/* 屏幕一帧显示内容(由 h_app 据分析/PLL 结果填好) */
typedef struct {
    uint32_t freq[H_NUM_SIGNALS];   /* 频率 Hz, [0]=低频A [1]=高频B */
    uint16_t amp[H_NUM_SIGNALS];    /* 相对幅度(FFT 幅值) */
    uint8_t  wave[H_NUM_SIGNALS];   /* 0=正弦 1=三角 (h_wave_t) */
    uint16_t phase_diff_deg;        /* B′−A′ 相位差, 折到 0..180 */
    uint8_t  locked;                /* 两路均锁定=1 */
} tjc_view_t;

/* ---- 单条指令构建(写入 buf, 不加 '\0', 返回字节数; 含结尾 3 个 0xFF; cap 不足则返回 0) ---- */
uint16_t TjcCore_val(char *buf, uint16_t cap, const char *obj, int32_t val);   /* obj.val=N */
uint16_t TjcCore_txt(char *buf, uint16_t cap, const char *obj, const char *s); /* obj.txt="s" */
uint16_t TjcCore_cmd(char *buf, uint16_t cap, const char *literal);            /* literal */

/* 波形码 -> 显示字符串 ("SINE"/"TRI"); 锁定标志 -> ("LOCK"/"UNLK") */
const char *TjcCore_waveStr(uint8_t wave);
const char *TjcCore_lockStr(uint8_t locked);

/* 增量构建整帧: 仅把 cur 中与 last 不同(或 force!=0)的字段拼成指令串写入 buf, 同步更新 last,
 * 返回总字节数. force=1 用于首帧(全发). 纯函数, 可离板验证. */
uint16_t TjcCore_buildView(char *buf, uint16_t cap, const tjc_view_t *cur,
                           tjc_view_t *last, uint8_t force);

/* ============================================================================
 *  发挥: 实时波形(addt 透传) + 触摸调相位 + 多页 —— 纯函数部分
 *  协议依据(官方 wiki 高置信): addt s0.id,ch,qty 透传(屏回0xFE就绪->连发qty裸字节->屏回0xFD完成);
 *    波形点 0~255(=距控件底像素高); 触摸返回 0x65 page comp event FFx3; sendme 回 0x66 page FFx3.
 *  ★控件命令必须用 .id (写 s0.id), 不能用对象名 s0(高置信 gotcha).
 * ========================================================================== */

#define TJC_OBJ_WAVE     "s0"       /* 波形控件对象名(命令里自动取 .id) */
#define TJC_PAGE_MAIN    0u         /* 主结果页(现有 n0~t2) */
#define TJC_PAGE_WAVE    1u         /* 波形页(s0) */
#define TJC_PAGE_SET     2u         /* 设置页(相位差) */

/* 波形点缩放: 12bit 码 x 按 [lo,hi] 线性映射到 0..255(clamp). hi<=lo 时返回中点 128(防除零).
 * 纯整数(M0+无FPU). 离板可测. */
uint8_t TjcCore_scale255(int32_t x, int32_t lo, int32_t hi);

/* 把 in_n 点采集块抽取降采样为 out_n 点并缩放到 0..255(供 addt 裸字节流). 纯函数, 离板可测. */
void TjcCore_decimateScale(const uint16_t *in, uint16_t in_n,
                           uint8_t *out, uint16_t out_n, int32_t lo, int32_t hi);

/* 构建 addt 命令头(不含裸数据): "addt <obj>.id,<ch>,<qty>" + FF FF FF, 返回字节数. */
uint16_t TjcCore_addtCmd(char *buf, uint16_t cap, const char *obj, uint8_t ch, uint16_t qty);

/* 逐点加一个波形点(addt 的慢速回退): "add <obj>.id,<ch>,<val>" + FF FF FF. val 0..255. */
uint16_t TjcCore_addPoint(char *buf, uint16_t cap, const char *obj, uint8_t ch, uint8_t val);

/* 清空波形通道: "cle <obj>.id,<ch>" + FF FF FF (ch=255 清该控件所有通道). */
uint16_t TjcCore_cle(char *buf, uint16_t cap, const char *obj, uint8_t ch);

/* ---- 屏 -> MCU 字节解析状态机(纯函数, 状态在调用方; 在 UART RX 中断里逐字节喂) ---- */
typedef enum {
    TJC_EVT_NONE = 0,
    TJC_EVT_TOUCH,        /* 0x65: 触摸键值 -> page/comp/event */
    TJC_EVT_PAGE,         /* 0x66: 当前页(sendme/页初始化) -> page */
    TJC_EVT_PHASE,        /* 自造帧 AA 5A <deg> 0D 0A: 相位目标 -> val(0..180) */
    TJC_EVT_WAVE_READY,   /* 0xFE: addt 透传就绪 */
    TJC_EVT_WAVE_DONE     /* 0xFD: addt 透传完成 */
} tjc_evt_type_t;

typedef struct {
    tjc_evt_type_t type;
    uint8_t page;     /* TOUCH/PAGE */
    uint8_t comp;     /* TOUCH 控件 id */
    uint8_t event;    /* TOUCH: 1=按下 0=弹起 */
    uint8_t val;      /* PHASE: 相位度 0..180 */
} tjc_rx_evt_t;

typedef struct {
    uint8_t state;    /* 解析状态 */
    uint8_t f[3];     /* 帧字节暂存(page/comp/event 或 phase) */
} tjc_rx_t;

/* 喂入一个字节; 若凑齐一帧返回 1 并填 out, 否则返回 0. 任意非法字节立刻回 IDLE 重同步(不挂死).
 * 0xFE/0xFD 在 IDLE 直接交付(其后续 FF FF FF 在 IDLE 被丢弃). 离板可测(本设计最大单测收益点). */
uint8_t TjcCore_parseRx(tjc_rx_t *st, uint8_t byte, tjc_rx_evt_t *out);

#endif /* TJC_HMI_CORE_H */
