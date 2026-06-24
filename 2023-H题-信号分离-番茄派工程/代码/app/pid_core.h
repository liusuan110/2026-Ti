#ifndef PID_CORE_H
#define PID_CORE_H

/* ============================================================================
 *  增量参考: 浮点 PID 控制器核(纯 C, 无硬件依赖, 可离板单元测试)
 *  用于"模拟鉴相 + PID"锁相路线(H_LOCK_METHOD == H_LOCK_ANALOG_PID, 见 h_config.h):
 *    外部模拟乘法器把 原信号 × AD9959 正交本振 -> 直流电压(=相位误差),
 *    ADC 读该电压作 feedback, 目标 target=0, PID 输出微调 DDS 频率把相位拉到正交锁定.
 *
 *  与学长参考工程(SignalSeparator_v2/User/PID)同构, 但 dt 作入参(我们用固定控制周期),
 *  且为纯函数便于 gcc 离板验证(见 host_test/test_pid.c). M0+ 无 FPU -> 软件浮点,
 *  每控制周期算两路, 开销可接受(不是逐采样).
 * ========================================================================== */

/* 双向限幅 */
#define PIDCORE_CLAMP(x, m)  (((x) < -(m)) ? -(m) : (((x) > (m)) ? (m) : (x)))

typedef struct {
    float kp;       /* 比例 */
    float ki;       /* 积分 */
    float kd;       /* 微分 */
    float int_max;  /* 积分累加双向限幅(抗饱和) */
    float out_max;  /* 输出双向限幅 */
} pid_param_t;

typedef struct {
    float target;     /* 目标值(模拟鉴相: 0) */
    float feedback;   /* 反馈值(鉴相电压 - 偏置) */
    float err_sum;    /* 误差累加 */
    float err_last;   /* 上次误差(微分用) */
    float out;        /* 最近输出 */
} pid_data_t;

/* 复位状态; target 为设定目标. */
void  PidCore_init(pid_data_t *d, float target);

/* 一步 PID: 用当前 d->feedback 与 d->target 算输出(写入 d->out 并返回).
 * dt 为本步时间间隔(s, >0); 积分项 ×dt、微分项 /dt(标准离散 PID). dt<=0 时跳过微分项. */
float PidCore_update(const pid_param_t *p, pid_data_t *d, float dt);

#endif /* PID_CORE_H */
