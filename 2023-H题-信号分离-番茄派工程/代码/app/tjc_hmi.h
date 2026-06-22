#ifndef TJC_HMI_H
#define TJC_HMI_H

#include "tjc_hmi_core.h"

/* ============================================================================
 *  淘晶驰串口屏 —— 硬件层(经 UART1 推送显示, 见 H_SignalSep.syscfg 的 UART_TJC)
 *  目标屏 TJC4827X243_011C(X2, 4.3", 480x272). H_TJC_DISPLAY=0 时整条链路关掉.
 *
 *  接线: MCU UART1_TX -> 屏 RXD; MCU UART1_RX -> 屏 TXD; 5V/GND 共地(引脚见 syscfg 生成宏).
 *  上位机: 在 USART HMI 编辑器的 page 0 建好控件(对象名见 tjc_hmi_core.h 表), 工程波特率 9600.
 * ========================================================================== */

void Tjc_init(void);                  /* 跳主页 page 0 + 标记首帧全发 */
void Tjc_show(const tjc_view_t *v);   /* 增量刷新: 只发与上次不同的字段 */

/* ---- 发挥: 波形 / 触摸 / 多页 ---- */
void    Tjc_rxEnable(void);           /* 开 UART1 RX 中断(触摸/相位/当前页/addt握手); RX关时为空操作 */
/* 把采集块抽取缩放后推到波形控件 s0(addt 透传, 带就绪超时防卡死). lo/hi 为本块动态量程. */
void    Tjc_pushWave(const uint16_t *block, uint16_t n, int32_t lo, int32_t hi);
/* 取屏上触摸设定的相位目标(度 0..180); 有新值返回 1 并写 *out_deg, 否则 0. */
uint8_t Tjc_takeTarget(int32_t *out_deg);
/* 当前显示页(0x66/sendme 跟踪); 0xFF=未知. 用于只在波形页推流(跨页 addt 无效). */
uint8_t Tjc_curPage(void);
/* 发 sendme 请屏上报当前页(屏回 0x66 更新 Tjc_curPage). 周期调用即可自发现当前页, 不依赖上位机配置. */
void    Tjc_queryPage(void);

#endif /* TJC_HMI_H */
