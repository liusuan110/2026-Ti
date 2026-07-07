#include "PID.h"

volatile uint16_t PID_Tick = 0;

void PID_getOutput(PID_Param_t* param, PID_Data_t* data) {
    // 获取时间间隔
    data->interval = (float)PID_Tick * 1E-3F;
    PID_Tick = 0;

    // 计算误差
    data->error = data->target - data->feedback;
    data->errorSum += data->error;
    data->errorSum = CONSTRAIN_2(data->errorSum, param->intMax);
    data->errorDiff = data->error - data->errorLast;
    data->errorLast = data->error;

    // 计算输出
    data->out = data->error * param->kp +
                data->errorSum * param->ki * data->interval +
                data->errorDiff * param->kd / data->interval;
    data->out = CONSTRAIN_2(data->out, param->outMax);
}
