/* 离板单元测试: 浮点 PID 核 pid_core (纯 C, PC/gcc).
 * gcc -std=c99 -I app -o t host_test/test_pid.c app/pid_core.c -lm && ./t */
#include <stdio.h>
#include <math.h>
#include "pid_core.h"

static int pass = 0, fail = 0;
static void check(const char *name, int cond, double got)
{
    if (cond) { printf("[PASS] %-34s (%.4f)\n", name, got); pass++; }
    else      { printf("[FAIL] %-34s (%.4f)\n", name, got); fail++; }
}

int main(void)
{
    printf("==== pid_core 离板测试 ====\n");

    /* --- 比例项: out = (target-feedback)*kp --- */
    {
        pid_param_t p = { .kp = 2.0f, .ki = 0.0f, .kd = 0.0f, .int_max = 1e6f, .out_max = 1e6f };
        pid_data_t  d; PidCore_init(&d, 0.0f);
        d.feedback = 1.0f;
        float out = PidCore_update(&p, &d, 0.005f);     /* err=-1 -> out=-2 */
        check("P项 out=-2", fabs(out + 2.0f) < 1e-5, out);
    }

    /* --- 输出限幅 --- */
    {
        pid_param_t p = { .kp = 100.0f, .ki = 0.0f, .kd = 0.0f, .int_max = 1e6f, .out_max = 5.0f };
        pid_data_t  d; PidCore_init(&d, 0.0f);
        d.feedback = 1.0f;
        float out = PidCore_update(&p, &d, 0.005f);     /* 想 -100 -> 限到 -5 */
        check("输出限幅 -5", fabs(out + 5.0f) < 1e-5, out);
    }

    /* --- 积分抗饱和: err_sum 限到 int_max --- */
    {
        pid_param_t p = { .kp = 0.0f, .ki = 1.0f, .kd = 0.0f, .int_max = 3.0f, .out_max = 1e6f };
        pid_data_t  d; PidCore_init(&d, 0.0f);
        int i;
        d.feedback = 10.0f;                              /* err=-10 每步 */
        for (i = 0; i < 50; i++) { PidCore_update(&p, &d, 0.005f); }
        check("积分限幅 -3", fabs(d.err_sum + 3.0f) < 1e-5, d.err_sum);
    }

    /* --- 闭环收敛: 含积分项, 模拟 PLL 整数器型对象 de/dt=(u-dist) --- */
    {
        pid_param_t p = { .kp = 0.5f, .ki = 0.2f, .kd = 0.0f, .int_max = 1000.0f, .out_max = 100.0f };
        pid_data_t  d; PidCore_init(&d, 0.0f);
        float e = 1.0f;          /* 初始相位误差 */
        float dist = 0.3f;       /* 恒定扰动(两晶振频差), 须靠积分抵消 */
        float dt = 0.005f, u;
        int i;
        for (i = 0; i < 4000; i++) {
            d.feedback = e;
            u = PidCore_update(&p, &d, dt);
            e += (u - dist) * dt;     /* 负反馈对象 */
        }
        check("闭环收敛 e->0", fabs(e) < 0.01, e);
        check("积分抵消扰动 u->dist", fabs(d.out - dist) < 0.02, d.out);
    }

    /* --- dt<=0 不除零(跳过微分) --- */
    {
        pid_param_t p = { .kp = 1.0f, .ki = 0.0f, .kd = 5.0f, .int_max = 1e6f, .out_max = 1e6f };
        pid_data_t  d; PidCore_init(&d, 0.0f);
        d.feedback = 1.0f;
        float out = PidCore_update(&p, &d, 0.0f);        /* dt=0 -> 无微分项, out=-1 */
        check("dt=0 无除零 out=-1", fabs(out + 1.0f) < 1e-5 && isfinite(out), out);
    }

    printf("\n==== %d/%d PASS ====\n", pass, pass + fail);
    return fail ? 1 : 0;
}
