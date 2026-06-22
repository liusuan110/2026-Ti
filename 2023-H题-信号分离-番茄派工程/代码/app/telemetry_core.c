#include "telemetry_core.h"

/* ---- 基础数 -> ASCII(自实现, 不用 printf/浮点) ---- */

uint8_t TelCore_u32(char *dst, uint32_t v)
{
    char tmp[10];
    uint8_t n = 0u, i;
    if (v == 0u) { dst[0] = '0'; return 1u; }
    while (v > 0u) { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; }
    for (i = 0u; i < n; i++) { dst[i] = tmp[n - 1u - i]; }  /* 反序 */
    return n;
}

uint8_t TelCore_i32(char *dst, int32_t v)
{
    if (v < 0) {
        dst[0] = '-';
        /* -v 用 uint32 取负, 避开 INT32_MIN 的 UB */
        return (uint8_t)(1u + TelCore_u32(&dst[1], (uint32_t)(-(int64_t)v)));
    }
    dst[0] = '+';
    return (uint8_t)(1u + TelCore_u32(&dst[1], (uint32_t)v));
}

int32_t TelCore_q31ToDeci(int32_t q31)
{
    /* ±2^31 = ±180.0° -> 十分之一度: deci = q31 * 1800 / 2^31 */
    return (int32_t)(((int64_t)q31 * 1800) / 2147483648LL);
}

uint8_t TelCore_deci(char *dst, int32_t deci)
{
    uint32_t a;
    uint8_t  p = 0u;
    if (deci < 0) { dst[p++] = '-'; a = (uint32_t)(-(int64_t)deci); }
    else          { dst[p++] = '+'; a = (uint32_t)deci; }
    p = (uint8_t)(p + TelCore_u32(&dst[p], a / 10u));   /* 整数部分 */
    dst[p++] = '.';
    dst[p++] = (char)('0' + (a % 10u));                 /* 一位小数 */
    return p;
}

void TelCore_bufStats(const uint16_t *buf, uint16_t n,
                      uint16_t *out_min, uint16_t *out_mean, uint16_t *out_max)
{
    uint16_t i, mn, mx;
    uint32_t sum;
    if (n == 0u) { *out_min = 0u; *out_mean = 0u; *out_max = 0u; return; }
    mn = buf[0]; mx = buf[0]; sum = 0u;
    for (i = 0u; i < n; i++) {
        uint16_t v = buf[i];
        if (v < mn) { mn = v; }
        if (v > mx) { mx = v; }
        sum += v;                       /* n<=1024, 4095*1024 < 2^32, 不溢出 */
    }
    *out_min  = mn;
    *out_max  = mx;
    *out_mean = (uint16_t)(sum / n);
}

/* ---- 受限追加助手(始终给 '\0' 留位, 安全截断) ---- */

static uint16_t put_s(char *dst, uint16_t cap, uint16_t pos, const char *s)
{
    while ((*s != '\0') && (pos < (uint16_t)(cap - 1u))) { dst[pos++] = *s++; }
    return pos;
}
static uint16_t put_u(char *dst, uint16_t cap, uint16_t pos, uint32_t v)
{
    char t[10]; uint8_t n = TelCore_u32(t, v), i;
    for (i = 0u; (i < n) && (pos < (uint16_t)(cap - 1u)); i++) { dst[pos++] = t[i]; }
    return pos;
}
static uint16_t put_deci(char *dst, uint16_t cap, uint16_t pos, int32_t deci)
{
    char t[14]; uint8_t n = TelCore_deci(t, deci), i;
    for (i = 0u; (i < n) && (pos < (uint16_t)(cap - 1u)); i++) { dst[pos++] = t[i]; }
    return pos;
}

uint16_t TelCore_formatLine(char *dst, uint16_t cap, const tel_frame_t *f)
{
    uint16_t p = 0u;
    uint8_t  s;
    if (cap == 0u) { return 0u; }

    /* 头: 帧 / 阶段 / 是否建链 */
    p = put_s(dst, cap, p, "F=");   p = put_u(dst, cap, p, f->frame);
    p = put_s(dst, cap, p, " S=");  p = put_u(dst, cap, p, (uint32_t)f->stage);
    p = put_s(dst, cap, p, " b=");  p = put_u(dst, cap, p, (uint32_t)f->built);
    /* ADC 原始 min/mean/max(看削顶/偏置/有无信号) */
    p = put_s(dst, cap, p, " C=");  p = put_u(dst, cap, p, f->c_min);
    p = put_s(dst, cap, p, "/");    p = put_u(dst, cap, p, f->c_mean);
    p = put_s(dst, cap, p, "/");    p = put_u(dst, cap, p, f->c_max);
    p = put_s(dst, cap, p, " FB="); p = put_u(dst, cap, p, f->fb_min);
    p = put_s(dst, cap, p, "/");    p = put_u(dst, cap, p, f->fb_mean);
    p = put_s(dst, cap, p, "/");    p = put_u(dst, cap, p, f->fb_max);

    /* 每路: 频率/波形/幅度/bin/相位/有效/重叠/PLL误差/锁定/FTW */
    for (s = 0u; s < (uint8_t)H_NUM_SIGNALS; s++) {
        p = put_s(dst, cap, p, " |");  p = put_u(dst, cap, p, (uint32_t)s);
        p = put_s(dst, cap, p, " f="); p = put_u(dst, cap, p, f->freq[s]);
        p = put_s(dst, cap, p, " w="); p = put_s(dst, cap, p, (f->wave[s] != 0u) ? "T" : "S");
        p = put_s(dst, cap, p, " a="); p = put_u(dst, cap, p, f->amp[s]);
        p = put_s(dst, cap, p, " k="); p = put_u(dst, cap, p, f->bin[s]);
        p = put_s(dst, cap, p, " ph=");p = put_deci(dst, cap, p, TelCore_q31ToDeci(f->phase_q31[s]));
        p = put_s(dst, cap, p, " v="); p = put_u(dst, cap, p, (uint32_t)f->valid[s]);
        p = put_s(dst, cap, p, " o="); p = put_u(dst, cap, p, (uint32_t)f->overlap[s]);
        p = put_s(dst, cap, p, " e="); p = put_deci(dst, cap, p, TelCore_q31ToDeci(f->perr_q31[s]));
        p = put_s(dst, cap, p, " L="); p = put_u(dst, cap, p, (uint32_t)f->locked[s]);
        p = put_s(dst, cap, p, " ftw=");p = put_u(dst, cap, p, f->ftw[s]);
    }

    if (p < (uint16_t)(cap - 1u)) { dst[p++] = '\n'; }
    dst[p] = '\0';
    return p;
}
