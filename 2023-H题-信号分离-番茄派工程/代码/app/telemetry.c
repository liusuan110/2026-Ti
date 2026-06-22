#include "telemetry.h"
#include "h_config.h"

#if H_TELEMETRY
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>

/* 一帧诊断行缓冲(静态, 不放栈). 容量足够当前字段; TelCore_formatLine 会安全截断. */
static char s_line[256];

void Tel_send(const char *s)
{
    while (*s != '\0') {
        DL_UART_transmitDataBlocking(UART_TEL_INST, (uint8_t)(*s));
        s++;
    }
}

void Tel_init(void)
{
    Tel_send("\r\n# H-SignalSep telemetry up. F=frame S=stage b=built "
             "C/FB=adc min/mean/max  |i f w a k ph v o e L ftw\r\n");
}

void Tel_line(const tel_frame_t *f)
{
    (void)TelCore_formatLine(s_line, (uint16_t)sizeof s_line, f);
    Tel_send(s_line);
}

#else  /* H_TELEMETRY == 0: 整条链路关掉, 留空实现 */
void Tel_send(const char *s)        { (void)s; }
void Tel_init(void)                 { }
void Tel_line(const tel_frame_t *f) { (void)f; }
#endif
