#ifndef __PID_H__
#define __PID_H__

#include <stdint.h>
#include "ti_msp_dl_config.h"

// 双向限幅
#define CONSTRAIN_2(x, max) (((x) < -(max)) ? -(max) : (((x) > (max)) ? (max) : (x)))

// PID控制参数
typedef struct PID_Param_t {
    float kp;     // 比例系数
    float ki;     // 积分系数
    float kd;     // 微分系数
    float intMax; // 积分最大值(双向)
    float outMax; // 输出最大值(双向)
} PID_Param_t;

// PID控制数据
typedef struct PID_Data_t {
    float target;    // 目标值
    float feedback;  // 反馈值
    float interval;  // 时间间隔(s)
    float error;     // 误差(error = target - feedback)
    float errorSum;  // 误差累加值
    float errorLast; // 上次误差
    float errorDiff; // 误差差分值
    float out;       // 控制量输出值
} PID_Data_t;

extern volatile uint16_t PID_Tick;

void PID_getOutput(PID_Param_t* param, PID_Data_t* data);

#endif /* #ifndef __PID_H__ */
