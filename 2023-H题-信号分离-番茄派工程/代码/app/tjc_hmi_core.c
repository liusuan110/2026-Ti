#include "tjc_hmi_core.h"
#include "telemetry_core.h"   /* TelCore_u32: 数字转 ASCII(复用, 不重复实现) */

/* ---- 内部小工具(纯 C, 不依赖 string.h) ---- */
static uint16_t slen(const char *s)
{
    uint16_t n = 0u;
    while (s[n] != '\0') { n++; }
    return n;
}
static uint16_t put(char *buf, uint16_t pos, const char *s)
{
    uint16_t i = 0u;
    while (s[i] != '\0') { buf[pos++] = s[i]; i++; }
    return pos;
}
static uint16_t term(char *buf, uint16_t pos)   /* 结尾三个 0xFF */
{
    buf[pos++] = (char)0xFFu;
    buf[pos++] = (char)0xFFu;
    buf[pos++] = (char)0xFFu;
    return pos;
}

const char *TjcCore_waveStr(uint8_t wave)   { return (wave   != 0u) ? "TRI"  : "SINE"; }
const char *TjcCore_lockStr(uint8_t locked) { return (locked != 0u) ? "LOCK" : "UNLK"; }

/* obj.val=N␏␏␏  —— 原子写入(放不下则一字节不写, 返回 0, 避免发出无结束符的残缺指令) */
uint16_t TjcCore_val(char *buf, uint16_t cap, const char *obj, int32_t val)
{
    char     num[12];
    uint8_t  nlen, neg = 0u, i;
    uint32_t mag;
    uint16_t need, p = 0u;

    if (val < 0) { neg = 1u; mag = (uint32_t)(-(int64_t)val); }
    else         { mag = (uint32_t)val; }
    nlen = TelCore_u32(num, mag);

    need = (uint16_t)(slen(obj) + 5u + neg + nlen + 3u);  /* obj + ".val=" + 符号 + 数字 + FF×3 */
    if (need > cap) { return 0u; }

    p = put(buf, p, obj);
    p = put(buf, p, ".val=");
    if (neg != 0u) { buf[p++] = '-'; }
    for (i = 0u; i < nlen; i++) { buf[p++] = num[i]; }
    return term(buf, p);
}

/* obj.txt="s"␏␏␏ */
uint16_t TjcCore_txt(char *buf, uint16_t cap, const char *obj, const char *s)
{
    uint16_t need, p = 0u;
    need = (uint16_t)(slen(obj) + 6u + slen(s) + 1u + 3u);  /* obj + ".txt=\"" + s + "\"" + FF×3 */
    if (need > cap) { return 0u; }

    p = put(buf, p, obj);
    p = put(buf, p, ".txt=\"");
    p = put(buf, p, s);
    buf[p++] = '"';
    return term(buf, p);
}

/* literal␏␏␏  (page N / dim=.. / cle .. 等任意整条指令) */
uint16_t TjcCore_cmd(char *buf, uint16_t cap, const char *literal)
{
    uint16_t need, p = 0u;
    need = (uint16_t)(slen(literal) + 3u);
    if (need > cap) { return 0u; }
    p = put(buf, p, literal);
    return term(buf, p);
}

/* 增量整帧: 只拼变化字段(或 force 全发). 任一字段放不下就停在已拼好的整指令边界返回(不残缺). */
uint16_t TjcCore_buildView(char *buf, uint16_t cap, const tjc_view_t *cur,
                           tjc_view_t *last, uint8_t force)
{
    static const char *fobj[2] = { TJC_OBJ_FREQ_A, TJC_OBJ_FREQ_B };
    static const char *aobj[2] = { TJC_OBJ_AMP_A,  TJC_OBJ_AMP_B  };
    static const char *wobj[2] = { TJC_OBJ_WAVE_A, TJC_OBJ_WAVE_B };
    uint16_t p = 0u, w;
    uint8_t  i;

    for (i = 0u; (i < (uint8_t)H_NUM_SIGNALS) && (i < 2u); i++) {
        if ((force != 0u) || (cur->freq[i] != last->freq[i])) {
            w = TjcCore_val(buf + p, (uint16_t)(cap - p), fobj[i], (int32_t)cur->freq[i]);
            if (w == 0u) { return p; }  p = (uint16_t)(p + w);  last->freq[i] = cur->freq[i];
        }
        if ((force != 0u) || (cur->amp[i] != last->amp[i])) {
            w = TjcCore_val(buf + p, (uint16_t)(cap - p), aobj[i], (int32_t)cur->amp[i]);
            if (w == 0u) { return p; }  p = (uint16_t)(p + w);  last->amp[i] = cur->amp[i];
        }
        if ((force != 0u) || (cur->wave[i] != last->wave[i])) {
            w = TjcCore_txt(buf + p, (uint16_t)(cap - p), wobj[i], TjcCore_waveStr(cur->wave[i]));
            if (w == 0u) { return p; }  p = (uint16_t)(p + w);  last->wave[i] = cur->wave[i];
        }
    }
    if ((force != 0u) || (cur->phase_diff_deg != last->phase_diff_deg)) {
        w = TjcCore_val(buf + p, (uint16_t)(cap - p), TJC_OBJ_PHASE, (int32_t)cur->phase_diff_deg);
        if (w == 0u) { return p; }  p = (uint16_t)(p + w);  last->phase_diff_deg = cur->phase_diff_deg;
    }
    if ((force != 0u) || (cur->locked != last->locked)) {
        w = TjcCore_txt(buf + p, (uint16_t)(cap - p), TJC_OBJ_LOCK, TjcCore_lockStr(cur->locked));
        if (w == 0u) { return p; }  p = (uint16_t)(p + w);  last->locked = cur->locked;
    }
    return p;
}

/* ===================== 波形 / 触摸 / 多页(纯函数) ===================== */

uint8_t TjcCore_scale255(int32_t x, int32_t lo, int32_t hi)
{
    if (hi <= lo) { return 128u; }                 /* 平直/无量程 -> 中点, 防除零 */
    if (x <= lo)  { return 0u;   }
    if (x >= hi)  { return 255u; }
    return (uint8_t)(((int64_t)(x - lo) * 255) / (hi - lo));
}

void TjcCore_decimateScale(const uint16_t *in, uint16_t in_n,
                           uint8_t *out, uint16_t out_n, int32_t lo, int32_t hi)
{
    uint16_t k;
    if ((out_n == 0u) || (in_n == 0u)) { return; }
    for (k = 0u; k < out_n; k++) {
        uint32_t idx = (uint32_t)k * in_n / out_n;             /* 抽取降采样 */
        if (idx >= in_n) { idx = (uint32_t)in_n - 1u; }
        out[k] = TjcCore_scale255((int32_t)in[idx], lo, hi);
    }
}

uint16_t TjcCore_addtCmd(char *buf, uint16_t cap, const char *obj, uint8_t ch, uint16_t qty)
{
    char     nch[4], nq[6];
    uint8_t  lch = TelCore_u32(nch, (uint32_t)ch), lq = TelCore_u32(nq, (uint32_t)qty), i;
    uint16_t need, p = 0u;
    need = (uint16_t)(5u + slen(obj) + 4u + lch + 1u + lq + 3u); /* "addt " obj ".id," ch "," qty FF×3 */
    if (need > cap) { return 0u; }
    p = put(buf, p, "addt ");
    p = put(buf, p, obj);
    p = put(buf, p, ".id,");
    for (i = 0u; i < lch; i++) { buf[p++] = nch[i]; }
    buf[p++] = ',';
    for (i = 0u; i < lq; i++) { buf[p++] = nq[i]; }
    return term(buf, p);
}

uint16_t TjcCore_addPoint(char *buf, uint16_t cap, const char *obj, uint8_t ch, uint8_t val)
{
    char     nch[4], nv[4];
    uint8_t  lch = TelCore_u32(nch, (uint32_t)ch), lv = TelCore_u32(nv, (uint32_t)val), i;
    uint16_t need, p = 0u;
    need = (uint16_t)(4u + slen(obj) + 4u + lch + 1u + lv + 3u);  /* "add " obj ".id," ch "," val FF×3 */
    if (need > cap) { return 0u; }
    p = put(buf, p, "add ");
    p = put(buf, p, obj);
    p = put(buf, p, ".id,");
    for (i = 0u; i < lch; i++) { buf[p++] = nch[i]; }
    buf[p++] = ',';
    for (i = 0u; i < lv; i++) { buf[p++] = nv[i]; }
    return term(buf, p);
}

uint16_t TjcCore_cle(char *buf, uint16_t cap, const char *obj, uint8_t ch)
{
    char     nch[4];
    uint8_t  lch = TelCore_u32(nch, (uint32_t)ch), i;
    uint16_t need, p = 0u;
    need = (uint16_t)(4u + slen(obj) + 4u + lch + 3u);          /* "cle " obj ".id," ch FF×3 */
    if (need > cap) { return 0u; }
    p = put(buf, p, "cle ");
    p = put(buf, p, obj);
    p = put(buf, p, ".id,");
    for (i = 0u; i < lch; i++) { buf[p++] = nch[i]; }
    return term(buf, p);
}

/* 解析状态(内部) */
enum {
    RX_IDLE = 0,
    RX_T_PAGE, RX_T_COMP, RX_T_EVT, RX_T_F1, RX_T_F2, RX_T_F3,   /* 0x65 触摸 */
    RX_P_PAGE, RX_P_F1, RX_P_F2, RX_P_F3,                        /* 0x66 当前页 */
    RX_PH_5A, RX_PH_VAL, RX_PH_CR, RX_PH_LF                      /* AA 5A val 0D 0A 相位 */
};

uint8_t TjcCore_parseRx(tjc_rx_t *st, uint8_t b, tjc_rx_evt_t *out)
{
    switch (st->state) {
    case RX_IDLE:
        if      (b == 0x65u) { st->state = RX_T_PAGE; }
        else if (b == 0x66u) { st->state = RX_P_PAGE; }
        else if (b == 0xAAu) { st->state = RX_PH_5A;  }
        else if (b == 0xFEu) { out->type = TJC_EVT_WAVE_READY; return 1u; }
        else if (b == 0xFDu) { out->type = TJC_EVT_WAVE_DONE;  return 1u; }
        return 0u;                                  /* 其它字节(含0xFF) 丢弃, 留 IDLE */

    /* 触摸: 0x65 page comp event FF FF FF */
    case RX_T_PAGE: st->f[0] = b; st->state = RX_T_COMP; return 0u;
    case RX_T_COMP: st->f[1] = b; st->state = RX_T_EVT;  return 0u;
    case RX_T_EVT:  st->f[2] = b; st->state = RX_T_F1;   return 0u;
    case RX_T_F1:   st->state = (b == 0xFFu) ? RX_T_F2 : RX_IDLE; return 0u;
    case RX_T_F2:   st->state = (b == 0xFFu) ? RX_T_F3 : RX_IDLE; return 0u;
    case RX_T_F3:
        st->state = RX_IDLE;
        if (b == 0xFFu) { out->type = TJC_EVT_TOUCH; out->page = st->f[0];
                          out->comp = st->f[1]; out->event = st->f[2]; return 1u; }
        return 0u;

    /* 当前页: 0x66 page FF FF FF */
    case RX_P_PAGE: st->f[0] = b; st->state = RX_P_F1; return 0u;
    case RX_P_F1:   st->state = (b == 0xFFu) ? RX_P_F2 : RX_IDLE; return 0u;
    case RX_P_F2:   st->state = (b == 0xFFu) ? RX_P_F3 : RX_IDLE; return 0u;
    case RX_P_F3:
        st->state = RX_IDLE;
        if (b == 0xFFu) { out->type = TJC_EVT_PAGE; out->page = st->f[0]; return 1u; }
        return 0u;

    /* 相位自造帧: AA 5A val 0D 0A */
    case RX_PH_5A:  st->state = (b == 0x5Au) ? RX_PH_VAL : RX_IDLE; return 0u;
    case RX_PH_VAL: st->f[0] = b; st->state = RX_PH_CR; return 0u;
    case RX_PH_CR:  st->state = (b == 0x0Du) ? RX_PH_LF : RX_IDLE; return 0u;
    case RX_PH_LF:
        st->state = RX_IDLE;
        if (b == 0x0Au) { out->type = TJC_EVT_PHASE; out->val = st->f[0]; return 1u; }
        return 0u;

    default: st->state = RX_IDLE; return 0u;
    }
}
