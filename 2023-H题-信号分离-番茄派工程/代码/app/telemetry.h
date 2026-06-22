#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include "telemetry_core.h"

/* ============================================================================
 *  上板遥测 —— 硬件层(经 UART 阻塞发送 ASCII 诊断)
 *  UART 由 SysConfig 配好(实例宏 UART_TEL_INST, 见 H_SignalSep.syscfg);
 *  本层只把 telemetry_core 拼好的文本逐字节推出去. 阻塞发送, 仅 bring-up 用,
 *  H_TELEMETRY=0 时整条链路可关掉(不占运行开销).
 *
 *  上板接线: 把 UART_TEL 的 TX 引脚(默认 PA10, 见 syscfg)接 USB-TTL 的 RX,
 *           GND 共地; 波特率见 syscfg(默认 115200 8N1). 串口助手即可看.
 * ========================================================================== */

void Tel_init(void);                       /* 发一行启动横幅(确认串口通) */
void Tel_send(const char *s);              /* 发一个以 '\0' 结尾的字符串 */
void Tel_line(const tel_frame_t *f);       /* 格式化并发送一整帧诊断行 */

#endif /* TELEMETRY_H */
