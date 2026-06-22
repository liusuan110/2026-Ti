/* 离板单元测试: 遥测格式化核 telemetry_core (纯 C, PC/gcc 跑).
 * gcc -std=c99 -I app -o t host_test/test_telemetry.c app/telemetry_core.c && ./t */
#include <stdio.h>
#include <string.h>
#include "telemetry_core.h"

static int pass = 0, fail = 0;

static void check_str(const char *name, const char *got, const char *exp)
{
    if (strcmp(got, exp) == 0) { printf("[PASS] %-22s '%s'\n", name, got); pass++; }
    else { printf("[FAIL] %-22s got '%s' exp '%s'\n", name, got, exp); fail++; }
}
static void check_int(const char *name, long got, long exp)
{
    if (got == exp) { printf("[PASS] %-22s %ld\n", name, got); pass++; }
    else { printf("[FAIL] %-22s got %ld exp %ld\n", name, got, exp); fail++; }
}
static void check_true(const char *name, int cond)
{
    if (cond) { printf("[PASS] %s\n", name); pass++; }
    else { printf("[FAIL] %s\n", name); fail++; }
}

static void fmt_u32(uint32_t v, char *out) { uint8_t n = TelCore_u32(out, v); out[n] = '\0'; }
static void fmt_i32(int32_t v, char *out)  { uint8_t n = TelCore_i32(out, v); out[n] = '\0'; }
static void fmt_dec(int32_t v, char *out)  { uint8_t n = TelCore_deci(out, v); out[n] = '\0'; }

int main(void)
{
    char b[64];

    printf("==== telemetry_core 离板测试 ====\n");

    /* --- 整数格式化 --- */
    fmt_u32(0u, b);          check_str("u32 0", b, "0");
    fmt_u32(50000u, b);      check_str("u32 50000", b, "50000");
    fmt_u32(4294967295u, b); check_str("u32 max", b, "4294967295");
    fmt_i32(0, b);           check_str("i32 0", b, "+0");
    fmt_i32(-123, b);        check_str("i32 -123", b, "-123");
    fmt_i32(2147483647, b);  check_str("i32 max", b, "+2147483647");
    fmt_i32(-2147483647-1, b); check_str("i32 min", b, "-2147483648");  /* 无 UB */

    /* --- Q31 角 -> 十分之一度 --- */
    check_int("q31 0 -> 0",          TelCore_q31ToDeci(0), 0);
    check_int("q31 +90deg -> 900",   TelCore_q31ToDeci((int32_t)(2147483648.0/2)), 900);
    check_int("q31 -90deg -> -900",  TelCore_q31ToDeci(-(int32_t)(2147483648.0/2)), -900);
    check_int("q31 +45deg -> 450",   TelCore_q31ToDeci((int32_t)(2147483648.0/4)), 450);

    /* --- 十分之一度 -> "+12.3" --- */
    fmt_dec(900, b);   check_str("deci +900", b, "+90.0");
    fmt_dec(-53, b);   check_str("deci -53",  b, "-5.3");
    fmt_dec(0, b);     check_str("deci 0",    b, "+0.0");
    fmt_dec(1805, b);  check_str("deci 1805", b, "+180.5");

    /* --- 缓冲统计 --- */
    {
        uint16_t buf[8] = {2048, 100, 4000, 2048, 2048, 50, 4090, 2048};
        uint16_t mn, me, mx;
        TelCore_bufStats(buf, 8, &mn, &me, &mx);
        check_int("bufStats min", mn, 50);
        check_int("bufStats max", mx, 4090);
        check_int("bufStats mean", me, (2048+100+4000+2048+2048+50+4090+2048)/8);
        TelCore_bufStats(buf, 0, &mn, &me, &mx);
        check_true("bufStats n=0 -> 0", mn==0 && me==0 && mx==0);
    }

    /* --- 整行组装 --- */
    {
        char line[256];
        tel_frame_t f;
        memset(&f, 0, sizeof f);
        f.frame = 7; f.stage = 4; f.built = 1;
        f.c_min = 20; f.c_mean = 2047; f.c_max = 4070;
        f.fb_min = 30; f.fb_mean = 2050; f.fb_max = 4060;
        f.freq[0] = 50000; f.wave[0] = 0; f.amp[0] = 18000; f.bin[0] = 50;
        f.phase_q31[0] = (int32_t)(2147483648.0/4); /* +45° -> +45.0 */
        f.valid[0] = 1; f.overlap[0] = 0;
        f.perr_q31[0] = 0; f.locked[0] = 1; f.ftw[0] = 429497;
        f.freq[1] = 100000; f.wave[1] = 1; f.amp[1] = 9000; f.bin[1] = 100;
        f.phase_q31[1] = -(int32_t)(2147483648.0/2); /* -90° -> -90.0 (整除, 避开截断歧义) */
        f.valid[1] = 1; f.overlap[1] = 1;
        f.perr_q31[1] = 0; f.locked[1] = 0; f.ftw[1] = 858993;
        uint16_t n = TelCore_formatLine(line, sizeof line, &f);

        check_true("line 非空且以\\n结尾", n > 0 && line[n-1] == '\n');
        check_true("含 F=7",     strstr(line, "F=7 ")    != NULL);
        check_true("含 S=4",     strstr(line, "S=4 ")    != NULL);
        check_true("含 C=20/2047/4070", strstr(line, "C=20/2047/4070") != NULL);
        check_true("含 f=50000", strstr(line, "f=50000 ")!= NULL);
        check_true("路0 w=S",    strstr(line, "|0 f=50000 w=S") != NULL);
        check_true("路1 w=T",    strstr(line, "|1 f=100000 w=T")!= NULL);
        check_true("含 ph=+45.0",strstr(line, "ph=+45.0") != NULL);
        check_true("含 ph=-90.0",strstr(line, "ph=-90.0") != NULL);
        check_true("含 L=1",     strstr(line, "L=1")      != NULL);
        check_true("含 o=1(重叠)",strstr(line, "o=1")     != NULL);
        printf("  组装样例: %s", line);

        /* 截断安全: 给极小 cap 不应越界(返回 < cap, 有 '\0') */
        char small[16];
        uint16_t m = TelCore_formatLine(small, sizeof small, &f);
        check_true("小cap安全截断", m < sizeof small && small[m] == '\0');
    }

    printf("\n==== %d/%d PASS ====\n", pass, pass + fail);
    return fail ? 1 : 0;
}
