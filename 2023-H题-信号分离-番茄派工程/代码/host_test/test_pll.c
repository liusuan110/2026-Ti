/* ============================================================================
 *  test_pll.c — 再生法软件锁相环 离板仿真/单元测试 (在 Windows 上用 gcc 跑)
 *
 *  目的: 不上板, 验证 pll_core 的 II 型数字 PLL 能:
 *    (1) 在两晶振 ppm 频差下锁定, 稳态相位误差 -> 0;
 *    (2) 按目标相位差(发挥②)锁定;
 *    (3) 从任意初相拉入;
 *    (4) 信号源重新调频后重锁;
 *    (5) 全程无整型溢出.
 *  并扫描增益(kp_shift/ki_shift)给出推荐值.
 *
 *  物理建模: 实时间步 dt. 源相位与 DDS 相位都在"墙钟时间"积分, 同一时刻测量,
 *           故 MSPM0 自身时钟误差在 (ref-fb) 差分中共模抵消, 无需建模.
 *           DDS 参考时钟带 ppm 误差 -> 即便用名义 500MHz 算出的 ftw_base 也会漂移.
 *
 *  编译(Windows): gcc -O2 -o test_pll.exe test_pll.c ..\app\pll_core.c ..\app\reconstruct_core.c -lm
 * ========================================================================== */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "pll_core.h"
#include "reconstruct_core.h"
#include "pll.h"          /* 取 PLL_KP_SHIFT / PLL_KI_SHIFT / PLL_INTEG_LIMIT */

#define FREF_NOM     500000000.0
#define TWO32        4294967296.0

static uint32_t cyc_to_u32(double cyc)
{
    double f = cyc - floor(cyc);          /* [0,1) */
    double v = f * TWO32;
    if (v >= TWO32) { v -= TWO32; }
    return (uint32_t)v;
}
static double perr_deg(int32_t p) { return (double)p / TWO32 * 360.0; }

/* 确定性伪随机噪声 [-amp, amp] (固定种子, 结果可复现) */
static uint32_t g_rng = 22222u;
static double noise(double amp)
{
    if (amp <= 0.0) { return 0.0; }
    g_rng = g_rng * 1664525u + 1013904223u;
    double u = (double)(g_rng >> 8) / (double)(1u << 24);   /* [0,1) */
    return (u * 2.0 - 1.0) * amp;
}

typedef struct {
    const char *name;
    double fA;            /* 源频率 Hz */
    double fA2; int step_at;  /* 在 step_at 步把源频率跳到 fA2(并 retune); <0 不跳 */
    double ppm;          /* DDS 参考时钟 ppm 误差 */
    double dt;           /* 环路周期 s */
    int    nsteps, settle;
    int    align;        /* 1: A' 初相对齐到 ref+target (build 时写 CPOW 的效果) */
    double target_deg;   /* 目标相位差 (发挥②) */
    double phaseA0_deg;  /* 源初相 */
    double noise_deg;    /* 相位测量噪声幅度 */
} scen_t;

/* 跑一个场景, 返回 settle 之后的最大 |perr|(deg); *locked_out=是否最终锁定 */
static double run_scen(const scen_t *s, int kp, int ki, int integ_limit,
                       int verbose, int *locked_out)
{
    uint32_t ftw_base = Recon_freqToFtw((uint32_t)llround(s->fA), 500000000u);
    pllcore_t pll;
    PllCore_init(&pll, ftw_base, kp, ki, integ_limit);

    double FREF_REAL = FREF_NOM * (1.0 + s->ppm * 1e-6);
    double phiA = s->phaseA0_deg / 360.0;
    double target_cyc = s->target_deg / 360.0;
    uint32_t target_u = cyc_to_u32(target_cyc);
    double phiD = s->align ? (phiA + target_cyc) : 0.0;
    uint32_t ftw = ftw_base;
    double maxpe = 0.0;
    int n;

    for (n = 0; n < s->nsteps; n++) {
        double fcur = (s->step_at >= 0 && n >= s->step_at) ? s->fA2 : s->fA;
        if (s->step_at >= 0 && n == s->step_at) {
            PllCore_retune(&pll, Recon_freqToFtw((uint32_t)llround(s->fA2), 500000000u));
        }
        /* 用当前 ftw 推进物理 dt */
        phiA += fcur * s->dt;
        double fdds = (double)ftw / TWO32 * FREF_REAL;
        phiD += fdds * s->dt;
        /* 同一时刻测量两路相位 */
        uint32_t ref = cyc_to_u32(phiA + noise(s->noise_deg / 360.0));
        uint32_t fb  = cyc_to_u32(phiD + noise(s->noise_deg / 360.0));
        /* 控制器一步 */
        ftw = PllCore_update(&pll, ref, fb, target_u);

        double pe = perr_deg(pll.last_perr);
        if (n >= s->settle) {
            double a = fabs(pe);
            if (a > maxpe) { maxpe = a; }
        }
        if (verbose && (n % verbose == 0)) {
            printf("    n=%5d perr=%+8.3f  ftw=%u  integ=%d  lock=%u\n",
                   n, pe, ftw, pll.freq_integ, pll.locked);
        }
    }
    if (locked_out) { *locked_out = (int)pll.locked; }
    return maxpe;
}

/* ---- 增益扫描: 用 ppm 跟踪场景, 找稳定且稳态误差最小的 (kp,ki) ---- */
static void gain_sweep(void)
{
    scen_t s = { "sweep", 100000.0, 0.0, -1, 25.0, 0.005, 3000, 2000,
                 1, 0.0, 30.0, 0.0 };
    int kp, ki, best_kp = 0, best_ki = 0;
    double best = 1e9;
    printf("=== 增益扫描 (fA=100k, ppm=+25, dt=5ms, 无噪) : 表内为 settle 后 max|perr| deg ===\n");
    printf("        ki:");
    for (ki = 24; ki <= 31; ki++) { printf("  %5d", ki); }
    printf("\n");
    for (kp = 18; kp <= 25; kp++) {
        printf("  kp=%2d :", kp);
        for (ki = 24; ki <= 31; ki++) {
            int lk;
            double m = run_scen(&s, kp, ki, (1 << 18), 0, &lk);
            if (m > 99.9) { printf("   div "); }
            else          { printf("  %5.2f", m); }
            if (lk && m < best) { best = m; best_kp = kp; best_ki = ki; }
        }
        printf("\n");
    }
    printf(">>> 推荐: kp_shift=%d ki_shift=%d (max|perr|=%.3f deg, 已锁定)\n",
           best_kp, best_ki, best);
    printf(">>> 当前 pll.h 烧入: kp_shift=%d ki_shift=%d\n\n",
           PLL_KP_SHIFT, PLL_KI_SHIFT);
}

static int g_pass = 0, g_fail = 0;
static void check(const char *what, double maxpe, int locked, double tol)
{
    int ok = (maxpe <= tol) && locked;
    printf("[%s] %-42s max|perr|=%6.3f deg  locked=%d  (tol=%.2f)\n",
           ok ? "PASS" : "FAIL", what, maxpe, locked, tol);
    if (ok) { g_pass++; } else { g_fail++; }
}

int main(void)
{
    int lk;
    double m;
    int KP = PLL_KP_SHIFT, KI = PLL_KI_SHIFT, IL = PLL_INTEG_LIMIT;

    gain_sweep();

    printf("=== 用 pll.h 烧入增益 (kp=%d ki=%d) 的场景验证 ===\n", KP, KI);

    /* 1) ppm 频差跟踪 (对齐起步), 不同频率/极性 */
    { scen_t s = { "", 100000.0, 0, -1, +20.0, 0.005, 3000, 2000, 1, 0.0, 30.0, 0.05 };
      m = run_scen(&s, KP, KI, IL, 0, &lk); check("ppm +20 @100k 跟踪", m, lk, 1.0); }
    { scen_t s = { "", 20000.0, 0, -1, -30.0, 0.005, 3000, 2000, 1, 0.0, 200.0, 0.05 };
      m = run_scen(&s, KP, KI, IL, 0, &lk); check("ppm -30 @20k 跟踪",  m, lk, 1.0); }
    { scen_t s = { "", 50000.0, 0, -1, +50.0, 0.005, 3000, 2000, 1, 0.0, 0.0, 0.05 };
      m = run_scen(&s, KP, KI, IL, 0, &lk); check("ppm +50 @50k 跟踪",  m, lk, 1.0); }

    /* 2) 目标相位差 (发挥②) */
    { scen_t s = { "", 50000.0, 0, -1, +15.0, 0.005, 3000, 2000, 1, 90.0, 10.0, 0.05 };
      m = run_scen(&s, KP, KI, IL, 0, &lk); check("锁定到目标相位差 90°", m, lk, 1.0); }

    /* 3) 从任意初相拉入 (不对齐起步) */
    { scen_t s = { "", 50000.0, 0, -1, +10.0, 0.005, 6000, 4000, 0, 0.0, 120.0, 0.05 };
      m = run_scen(&s, KP, KI, IL, 0, &lk); check("从120°初相拉入并锁定", m, lk, 1.0); }

    /* 4) 信号源重新调频 50k -> 55k, 中途 retune 后重锁 */
    { scen_t s = { "", 50000.0, 55000.0, 2000, +20.0, 0.005, 6000, 4500, 1, 0.0, 30.0, 0.05 };
      m = run_scen(&s, KP, KI, IL, 0, &lk); check("频率跳变 50k->55k 重锁", m, lk, 1.0); }

    /* 5) 对比: 同增益但 dt=50ms (当前阻塞延时) —— 预期变差, 佐证 SYS-4 应修 */
    { scen_t s = { "", 100000.0, 0, -1, +20.0, 0.050, 3000, 2000, 1, 0.0, 30.0, 0.05 };
      int lk2; double m2 = run_scen(&s, KP, KI, IL, 0, &lk2);
      printf("[INFO] 同增益 dt=50ms @100k: max|perr|=%.3f deg locked=%d "
             "(若明显变差/失锁, 说明应按SYS-4去掉50ms延时, 或对50ms重整定增益)\n", m2, lk2); }

    printf("\n=== 结果: %d 通过, %d 失败 ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
