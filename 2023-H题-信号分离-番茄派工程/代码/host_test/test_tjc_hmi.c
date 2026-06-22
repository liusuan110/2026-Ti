/* 离板单元测试: 淘晶驰串口屏指令构建核 tjc_hmi_core (纯 C, PC/gcc).
 * gcc -std=c99 -I app -o t host_test/test_tjc_hmi.c app/tjc_hmi_core.c app/telemetry_core.c && ./t */
#include <stdio.h>
#include <string.h>
#include "tjc_hmi_core.h"

static int pass = 0, fail = 0;

/* 字节级比对(指令含 0xFF 但无内嵌 0x00, 故 strlen 可得预期字节数, 免手数出错) */
static void eq_bytes(const char *name, const char *got, uint16_t glen, const char *exp)
{
    uint16_t elen = (uint16_t)strlen(exp);
    int ok = (glen == elen) && (memcmp(got, exp, glen) == 0);
    if (ok) { printf("[PASS] %s (%u 字节)\n", name, glen); pass++; }
    else {
        printf("[FAIL] %s: got %u 字节 exp %u 字节\n       got=[", name, glen, elen);
        for (uint16_t i = 0; i < glen; i++) printf("%02X ", (unsigned char)got[i]);
        printf("]\n       exp=[");
        for (uint16_t i = 0; i < elen; i++) printf("%02X ", (unsigned char)exp[i]);
        printf("]\n");
        fail++;
    }
}
static void t_true(const char *name, int cond)
{
    if (cond) { printf("[PASS] %s\n", name); pass++; }
    else { printf("[FAIL] %s\n", name); fail++; }
}
/* 统计 buf 中的 0xFF 三连(= 指令条数) */
static int count_cmds(const char *buf, uint16_t len)
{
    int c = 0; uint16_t i = 0;
    while (i + 3 <= len) {
        if ((unsigned char)buf[i]==0xFF && (unsigned char)buf[i+1]==0xFF && (unsigned char)buf[i+2]==0xFF) { c++; i += 3; }
        else i++;
    }
    return c;
}
static int contains(const char *buf, uint16_t len, const char *sub)
{
    uint16_t sl = (uint16_t)strlen(sub), i;
    if (sl > len) return 0;
    for (i = 0; i + sl <= len; i++) if (memcmp(buf+i, sub, sl) == 0) return 1;
    return 0;
}

int main(void)
{
    char buf[512];
    uint16_t n;

    printf("==== tjc_hmi_core 离板测试 ====\n");

    /* --- 单条指令字节精确 --- */
    n = TjcCore_val(buf, sizeof buf, "n0", 50000);
    eq_bytes("val n0=50000", buf, n, "n0.val=50000\xFF\xFF\xFF");

    n = TjcCore_txt(buf, sizeof buf, "t0", "SINE");
    eq_bytes("txt t0=SINE", buf, n, "t0.txt=\"SINE\"\xFF\xFF\xFF");

    n = TjcCore_cmd(buf, sizeof buf, "page 0");
    eq_bytes("cmd page 0", buf, n, "page 0\xFF\xFF\xFF");

    n = TjcCore_val(buf, sizeof buf, "n4", -5);
    eq_bytes("val 负数 n4=-5", buf, n, "n4.val=-5\xFF\xFF\xFF");

    /* --- 容量不足: 原子返回 0, 不写残缺 --- */
    n = TjcCore_val(buf, 8, "n0", 50000);   /* 需 15 字节, 给 8 */
    t_true("cap 不足返回0(不残缺)", n == 0);

    /* --- 波形/锁定字符串 --- */
    t_true("wave 0 -> SINE", strcmp(TjcCore_waveStr(0), "SINE") == 0);
    t_true("wave 1 -> TRI",  strcmp(TjcCore_waveStr(1), "TRI")  == 0);
    t_true("lock 1 -> LOCK", strcmp(TjcCore_lockStr(1), "LOCK") == 0);
    t_true("lock 0 -> UNLK", strcmp(TjcCore_lockStr(0), "UNLK") == 0);

    /* --- 增量整帧 --- */
    {
        tjc_view_t cur, last;
        memset(&last, 0xAA, sizeof last);   /* last 置成不可能值, force 首帧全发 */
        cur.freq[0]=50000; cur.amp[0]=18000; cur.wave[0]=0;
        cur.freq[1]=100000; cur.amp[1]=9000; cur.wave[1]=1;
        cur.phase_diff_deg=90; cur.locked=1;

        n = TjcCore_buildView(buf, sizeof buf, &cur, &last, 1u);  /* 首帧全发 */
        t_true("首帧 8 条指令", count_cmds(buf, n) == 8);
        t_true("含 n0.val=50000",  contains(buf, n, "n0.val=50000"));
        t_true("含 t0.txt=\"SINE\"", contains(buf, n, "t0.txt=\"SINE\""));
        t_true("含 t1.txt=\"TRI\"",  contains(buf, n, "t1.txt=\"TRI\""));
        t_true("含 n2.val=100000", contains(buf, n, "n2.val=100000"));
        t_true("含 n4.val=90",     contains(buf, n, "n4.val=90"));
        t_true("含 t2.txt=\"LOCK\"", contains(buf, n, "t2.txt=\"LOCK\""));

        /* 同帧再发: 无变化 -> 0 字节(这是省带宽的关键) */
        n = TjcCore_buildView(buf, sizeof buf, &cur, &last, 0u);
        t_true("无变化 -> 0 字节", n == 0);

        /* 只改一个字段 -> 只发该字段一条指令 */
        cur.freq[0] = 55000;
        n = TjcCore_buildView(buf, sizeof buf, &cur, &last, 0u);
        t_true("改 freq[0] -> 1 条指令", count_cmds(buf, n) == 1);
        t_true("且为 n0.val=55000", contains(buf, n, "n0.val=55000"));

        /* 改波形与锁定 -> 2 条文本指令 */
        cur.wave[1] = 0; cur.locked = 0;
        n = TjcCore_buildView(buf, sizeof buf, &cur, &last, 0u);
        t_true("改 wave/lock -> 2 条", count_cmds(buf, n) == 2);
        t_true("含 t1.txt=\"SINE\"", contains(buf, n, "t1.txt=\"SINE\""));
        t_true("含 t2.txt=\"UNLK\"", contains(buf, n, "t2.txt=\"UNLK\""));
    }

    /* --- 波形: scale255 边界 --- */
    t_true("scale lo边界->0",   TjcCore_scale255(800, 800, 3200) == 0);
    t_true("scale hi边界->255", TjcCore_scale255(3200, 800, 3200) == 255);
    t_true("scale 中点->127",   TjcCore_scale255(2000, 800, 3200) == 127);
    t_true("scale 越下界->0",   TjcCore_scale255(0, 800, 3200) == 0);
    t_true("scale 越上界->255", TjcCore_scale255(9999, 800, 3200) == 255);
    t_true("scale hi<=lo->128", TjcCore_scale255(2000, 2048, 2048) == 128);

    /* --- 波形: decimateScale 抽取+缩放 --- */
    {
        uint16_t in[16]; uint8_t out[4]; int i;
        for (i = 0; i < 16; i++) in[i] = (uint16_t)(i * 273);   /* 0..4095 斜坡 */
        TjcCore_decimateScale(in, 16, out, 4, 0, 4095);
        /* idx = k*16/4 = 0,4,8,12 -> in=0,1092,2184,3276 -> scale255 */
        t_true("decimate[0]=0",   out[0] == 0);
        t_true("decimate递增",    out[0] < out[1] && out[1] < out[2] && out[2] < out[3]);
        t_true("decimate[3]合理", out[3] > 190);   /* uint8 天然 <=255 */
    }

    /* --- addt / cle 命令字节 --- */
    n = TjcCore_addtCmd(buf, sizeof buf, "s0", 0, 200);
    eq_bytes("addt s0.id,0,200", buf, n, "addt s0.id,0,200\xFF\xFF\xFF");
    n = TjcCore_addPoint(buf, sizeof buf, "s0", 0, 255);
    eq_bytes("add s0.id,0,255", buf, n, "add s0.id,0,255\xFF\xFF\xFF");
    n = TjcCore_cle(buf, sizeof buf, "s0", 0);
    eq_bytes("cle s0.id,0", buf, n, "cle s0.id,0\xFF\xFF\xFF");
    n = TjcCore_cle(buf, sizeof buf, "s0", 255);
    eq_bytes("cle s0.id,255", buf, n, "cle s0.id,255\xFF\xFF\xFF");
    /* page/sendme 复用 TjcCore_cmd */
    n = TjcCore_cmd(buf, sizeof buf, "page 1");
    eq_bytes("page 1", buf, n, "page 1\xFF\xFF\xFF");
    n = TjcCore_cmd(buf, sizeof buf, "sendme");
    eq_bytes("sendme", buf, n, "sendme\xFF\xFF\xFF");

    /* --- 解析状态机 parseRx(最大离板收益): 喂字节序列 --- */
    {
        tjc_rx_t st; tjc_rx_evt_t e; int got;
        const unsigned char *p; int i;
        #define FEED(seq, len) do { got = 0; p = (const unsigned char*)(seq); \
            for (i = 0; i < (len); i++) { if (TjcCore_parseRx(&st, p[i], &e)) got = 1; } } while (0)

        st.state = 0;
        /* 触摸: 65 00 03 00 FF FF FF -> 控件3 弹起 */
        FEED("\x65\x00\x03\x00\xFF\xFF\xFF", 7);
        t_true("touch 交付", got && e.type == TJC_EVT_TOUCH);
        t_true("touch comp=3", e.comp == 3 && e.event == 0 && e.page == 0);

        /* 当前页: 66 01 FF FF FF -> page 1 */
        FEED("\x66\x01\xFF\xFF\xFF", 5);
        t_true("page 交付", got && e.type == TJC_EVT_PAGE && e.page == 1);

        /* 相位自造帧: AA 5A 5A 0D 0A -> 90° */
        FEED("\xAA\x5A\x5A\x0D\x0A", 5);
        t_true("phase 交付", got && e.type == TJC_EVT_PHASE && e.val == 90);

        /* addt 就绪/完成: FE / FD (其后 FF FF FF 在 IDLE 丢弃) */
        FEED("\xFE\xFF\xFF\xFF", 4);
        t_true("wave ready", got && e.type == TJC_EVT_WAVE_READY);
        FEED("\xFD\xFF\xFF\xFF", 4);
        t_true("wave done", got && e.type == TJC_EVT_WAVE_DONE);

        /* 半截帧(0x65 没收完就来噪声)不挂死, 之后正常帧仍能解析 */
        FEED("\x65\x00\x12", 3);                 /* 半截触摸帧 */
        t_true("半截帧未交付", !got);
        FEED("\x99\x88\x77", 3);                 /* 噪声 */
        FEED("\x66\x02\xFF\xFF\xFF", 5);         /* 紧跟正常页帧仍解析(重同步) */
        t_true("噪声后重同步", got && e.type == TJC_EVT_PAGE && e.page == 2);

        /* 触摸帧尾 FF 缺失 -> 不误交付, 且能继续 */
        FEED("\x65\x00\x05\x01\xFF\x00\xFF", 7); /* 第2个尾字节是0x00 -> 回IDLE */
        t_true("坏尾不误交付touch", !(got && e.type==TJC_EVT_TOUCH));
        #undef FEED
    }

    printf("\n==== %d/%d PASS ====\n", pass, pass + fail);
    return fail ? 1 : 0;
}
