#include "tjc_hmi.h"
#include "h_config.h"

#if H_TJC_DISPLAY
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include "Tick.h"

/* RX 是否需要编译: 触摸开, 或 波形用 addt(需收 0xFE 就绪/0xFD 完成握手帧) */
#define H_TJC_RX_ON  (H_TJC_TOUCH || (H_TJC_WAVE && H_TJC_WAVE_USE_ADDT))

static char       s_buf[256];      /* 结果页增量帧缓冲 */
static tjc_view_t s_last;          /* 上次已发的显示内容 */
static uint8_t    s_first = 1u;    /* 首帧全发标志 */

static void tjc_send(const char *b, uint16_t n)
{
    uint16_t i;
    for (i = 0u; i < n; i++) {
        DL_UART_transmitDataBlocking(UART_TJC_INST, (uint8_t)b[i]);
    }
}

void Tjc_init(void)
{
    char     c[16];
    uint16_t n = TjcCore_cmd(c, (uint16_t)sizeof c, "page 0");  /* 切到主显示页 */
    tjc_send(c, n);
    s_first = 1u;
}

void Tjc_show(const tjc_view_t *v)
{
    uint16_t n = TjcCore_buildView(s_buf, (uint16_t)sizeof s_buf, v, &s_last, s_first);
    if (n != 0u) { tjc_send(s_buf, n); }
    s_first = 0u;
}

void Tjc_queryPage(void)
{
    char     c[12];
    uint16_t n = TjcCore_cmd(c, (uint16_t)sizeof c, "sendme");  /* 屏回 0x66 <page> 更新当前页 */
    tjc_send(c, n);
}

/* -------------------- RX: 触摸 / 相位 / 当前页 / addt 握手 -------------------- */
#if H_TJC_RX_ON
static tjc_rx_t         s_rx;
static volatile uint8_t s_cur_page   = 0xFFu;   /* 0x66/sendme 跟踪 */
static volatile int32_t s_pending_deg = -1;     /* 触摸相位待取, -1=无 */
static volatile uint8_t s_wave_ready = 0u;      /* addt 就绪(0xFE) */
static volatile uint8_t s_wave_done  = 0u;      /* addt 完成(0xFD) */

/* UART1 RX 中断: 逐字节喂解析状态机, 事件落到 volatile(中断里不发送/不长运算) */
void UART_TJC_INST_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(UART_TJC_INST)) {
        case DL_UART_IIDX_RX: {
            uint8_t      b = (uint8_t)DL_UART_receiveData(UART_TJC_INST);
            tjc_rx_evt_t e;
            if (TjcCore_parseRx(&s_rx, b, &e) != 0u) {
                switch (e.type) {
                    case TJC_EVT_PHASE:      s_pending_deg = (int32_t)e.val; break;
                    case TJC_EVT_PAGE:       s_cur_page    = e.page;         break;
                    case TJC_EVT_WAVE_READY: s_wave_ready  = 1u;             break;
                    case TJC_EVT_WAVE_DONE:  s_wave_done   = 1u;             break;
                    default: break;   /* TOUCH 键值帧: 相位用自造帧, 此处不单独处理 */
                }
            }
            break;
        }
        default: break;
    }
}

void Tjc_rxEnable(void)
{
    s_rx.state = 0u;
    DL_UART_enableInterrupt(UART_TJC_INST, DL_UART_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_TJC_INST_INT_IRQN);
}

uint8_t Tjc_takeTarget(int32_t *out_deg)
{
    int32_t d = s_pending_deg;          /* 单生产者(ISR)/单消费者(主循环), int32 原子读 */
    if (d < 0) { return 0u; }
    s_pending_deg = -1;
    *out_deg = d;
    return 1u;
}

uint8_t Tjc_curPage(void) { return s_cur_page; }
#else
void    Tjc_rxEnable(void)               { }
uint8_t Tjc_takeTarget(int32_t *out_deg) { (void)out_deg; return 0u; }
uint8_t Tjc_curPage(void)                { return 0xFFu; }
#endif  /* H_TJC_RX_ON */

/* -------------------- 波形推流 -------------------- */
#if H_TJC_WAVE
static uint8_t s_pts[H_TJC_WAVE_N];   /* 抽取+缩放后的 0..255 点 */

void Tjc_pushWave(const uint16_t *block, uint16_t n, int32_t lo, int32_t hi)
{
    uint16_t k;
    TjcCore_decimateScale(block, n, s_pts, (uint16_t)H_TJC_WAVE_N, lo, hi);

#if H_TJC_WAVE_USE_ADDT
    {
        char     cmd[28];
        uint16_t clen = TjcCore_addtCmd(cmd, (uint16_t)sizeof cmd, TJC_OBJ_WAVE,
                                        0u, (uint16_t)H_TJC_WAVE_N);
        uint32_t t0;
        s_wave_ready = 0u; s_wave_done = 0u;
        tjc_send(cmd, clen);
        /* 等屏回就绪帧 0xFE(超时则放弃本帧; 此时屏尚未进透传态, 安全不卡死) */
        t0 = Tick;
        while (s_wave_ready == 0u) {
            __WFE();
            if ((uint32_t)(Tick - t0) > (uint32_t)H_TJC_WAVE_TIMEOUT_MS) { return; }
        }
        /* 精确连发 N 个裸字节: 少发会让屏永久卡在透传态, 故严格 N 次, 每字节即一点 0..255 */
        for (k = 0u; k < (uint16_t)H_TJC_WAVE_N; k++) {
            DL_UART_transmitDataBlocking(UART_TJC_INST, s_pts[k]);
        }
        /* 屏收满后自动回 0xFD 并重绘曲线; 不强等完成帧 */
    }
#else  /* add 逐点(慢速回退: addt 上板验不通时用, 建议同时调小 H_TJC_WAVE_N) */
    {
        char cmd[24];
        for (k = 0u; k < (uint16_t)H_TJC_WAVE_N; k++) {
            uint16_t clen = TjcCore_addPoint(cmd, (uint16_t)sizeof cmd, TJC_OBJ_WAVE, 0u, s_pts[k]);
            tjc_send(cmd, clen);
        }
    }
#endif
}
#else
void Tjc_pushWave(const uint16_t *block, uint16_t n, int32_t lo, int32_t hi)
{ (void)block; (void)n; (void)lo; (void)hi; }
#endif  /* H_TJC_WAVE */

#else  /* H_TJC_DISPLAY == 0: 全部空实现 */
void Tjc_init(void)                              { }
void Tjc_show(const tjc_view_t *v)               { (void)v; }
void Tjc_queryPage(void)                         { }
void Tjc_rxEnable(void)                          { }
void Tjc_pushWave(const uint16_t *b, uint16_t n, int32_t lo, int32_t hi)
{ (void)b; (void)n; (void)lo; (void)hi; }
uint8_t Tjc_takeTarget(int32_t *out_deg)         { (void)out_deg; return 0u; }
uint8_t Tjc_curPage(void)                        { return 0xFFu; }
#endif  /* H_TJC_DISPLAY */
