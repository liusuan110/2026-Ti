#include "pid_core.h"

void PidCore_init(pid_data_t *d, float target)
{
    d->target   = target;
    d->feedback = 0.0f;
    d->err_sum  = 0.0f;
    d->err_last = 0.0f;
    d->out      = 0.0f;
}

float PidCore_update(const pid_param_t *p, pid_data_t *d, float dt)
{
    float err      = d->target - d->feedback;
    float err_diff = err - d->err_last;
    float out;

    /* 积分: 累加并抗饱和限幅 */
    d->err_sum += err;
    d->err_sum  = PIDCORE_CLAMP(d->err_sum, p->int_max);
    d->err_last = err;

    /* P + I(×dt) + D(/dt) */
    out = err * p->kp + d->err_sum * p->ki * dt;
    if (dt > 0.0f) {
        out += err_diff * p->kd / dt;
    }
    out      = PIDCORE_CLAMP(out, p->out_max);
    d->out   = out;
    return out;
}
