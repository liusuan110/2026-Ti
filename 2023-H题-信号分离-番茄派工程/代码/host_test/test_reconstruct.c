/* 离板验证 reconstruct_core 的 AD9959 数值换算.
 * cc -std=c99 -I ../app -o testr test_reconstruct.c ../app/reconstruct_core.c && ./testr */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "reconstruct_core.h"

static int g_pass = 0, g_total = 0;

static void chk_u32(const char *name, uint32_t got, uint32_t exp)
{
    g_total++;
    int ok = (got == exp);
    if (ok) g_pass++;
    printf("  %-28s got=%-10u exp=%-10u %s\n", name, got, exp, ok ? "PASS" : "FAIL");
}
static void chk_u16(const char *name, uint16_t got, uint16_t exp)
{
    g_total++;
    int ok = (got == exp);
    if (ok) g_pass++;
    printf("  %-28s got=%-6u exp=%-6u %s\n", name, got, exp, ok ? "PASS" : "FAIL");
}

int main(void)
{
    int deg;
    double max_err = 0.0;

    printf("== freq -> FTW (sysclk=500MHz) ==\n");
    chk_u32("50kHz",   Recon_freqToFtw(50000u,   500000000u), 429497u);
    chk_u32("100kHz",  Recon_freqToFtw(100000u,  500000000u), 858993u);
    chk_u32("20kHz",   Recon_freqToFtw(20000u,   500000000u), 171799u);
    chk_u32("1MHz",    Recon_freqToFtw(1000000u, 500000000u), 8589935u);

    printf("== 归一化角 Q31 -> CPOW(14位) ==\n");
    chk_u16("0deg",    Recon_angleQ31ToCpow(0x00000000),         0u);
    chk_u16("45deg",   Recon_angleQ31ToCpow(0x20000000),      2048u);
    chk_u16("90deg",   Recon_angleQ31ToCpow(0x40000000),      4096u);
    chk_u16("135deg",  Recon_angleQ31ToCpow(0x60000000),      6144u);
    chk_u16("~180deg", Recon_angleQ31ToCpow(0x7FFFFFFF),      8191u);
    chk_u16("-90deg(270)",  Recon_angleQ31ToCpow((int32_t)0xC0000000), 12288u);
    chk_u16("-45deg(315)",  Recon_angleQ31ToCpow((int32_t)0xE0000000), 14336u);

    printf("== 角度(度) -> CPOW (发挥②相位差) ==\n");
    chk_u16("0",   Recon_degToCpow(0),       0u);
    chk_u16("5",   Recon_degToCpow(5),     228u);
    chk_u16("45",  Recon_degToCpow(45),   2048u);
    chk_u16("90",  Recon_degToCpow(90),   4096u);
    chk_u16("180", Recon_degToCpow(180),  8192u);
    chk_u16("355", Recon_degToCpow(355), 16156u);

    /* 发挥②: 0..180°, 5° 步进, 复原误差应 << 5° (要求绝对误差<=5°) */
    printf("== 相位差 0..180 步进5 复原误差 ==\n");
    for (deg = 0; deg <= 180; deg += 5) {
        uint16_t cp = Recon_degToCpow(deg);
        double actual = (double)cp * 360.0 / 16384.0;
        double err = fabs(actual - (double)deg);
        if (err > max_err) max_err = err;
    }
    g_total++;
    if (max_err < 0.05) { g_pass++; printf("  max_err=%.4f deg (<0.05) PASS\n", max_err); }
    else { printf("  max_err=%.4f deg FAIL\n", max_err); }

    printf("\n==== %d/%d PASS ====\n", g_pass, g_total);
    return (g_pass == g_total) ? 0 : 1;
}
