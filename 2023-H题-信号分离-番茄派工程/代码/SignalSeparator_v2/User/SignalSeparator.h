#ifndef __SIGNAL_SEPARATOR_H__
#define __SIGNAL_SEPARATOR_H__

#include <stdint.h>
#include <math.h>
#include "ti_msp_dl_config.h"
#include "UserADC.h"
#include "SpectrumAnalyzer.h"
#include "PID.h"

#define SIG_SEP_SA_TIME 500 // 频谱分析时间间隔(ms)
#define SIG_SEP_PLL_TIME 10 // DDS更新时间间隔(ms)
#define SIG_SEP_PHASE_SHIFT_TIME 200 // 相位差更新时间间隔(ms)

#define RLY_A_ON  (DL_GPIO_setPins(GPIO_RLY_PORT, GPIO_RLY_SIG_A_PIN))
#define RLY_A_OFF (DL_GPIO_clearPins(GPIO_RLY_PORT, GPIO_RLY_SIG_A_PIN))
#define RLY_B_ON  (DL_GPIO_setPins(GPIO_RLY_PORT, GPIO_RLY_SIG_B_PIN))
#define RLY_B_OFF (DL_GPIO_clearPins(GPIO_RLY_PORT, GPIO_RLY_SIG_B_PIN))

#define ABS(x) (((x) < 0) ? (-(x)) : (x))

// 信号分离装置参数
typedef struct SigSep_Param_t {
    uint32_t fs; // 采样率(Sa/s)
    q15_t sigOffset; // 输入信号偏置(ADC采样值)
    float PDOffset; // 鉴相器输出信号偏置(V)
} SigSep_Param_t;

// 波形类型
typedef enum SigType_t {
    SIG_TYPE_SINE,    // 正弦波
    SIG_TYPE_TRIANGLE // 三角波
} SigType_t;

// 信号数据
typedef struct SigData_t {
    float freq;     // 频率
    SigType_t type; // 类型
} SigData_t;

typedef struct SigSep_Data_t {
    SigData_t sigA;
    SigData_t sigB;
    float basebandA;
    float basebandB;
} SigSep_Data_t;

extern q15_t MagData[FFT_SIZE];
extern SA_SpectrumData_t SpectrumPeaks[SPECTRUM_N_PEAKS_MAX];
extern uint16_t SpectrumPeaksN;
extern PID_Param_t PID_Param_50kHz;
extern PID_Param_t PID_ParamA;
extern PID_Param_t PID_ParamB;
extern PID_Data_t PID_DataA;
extern PID_Data_t PID_DataB;
extern volatile uint16_t SigSep_SATick;
extern volatile uint16_t SigSep_PLLTick;
extern volatile uint16_t SigSep_PhaseShiftTick;

void SigSep_init(SigSep_Param_t* param, SigSep_Data_t* data);
void SigSep_taskSpectrumAnalysis(SigSep_Param_t* param, SigSep_Data_t* data);
void SigSep_taskPLL(SigSep_Param_t* param, SigSep_Data_t* data);
void SigSep_taskPhaseShift(void);
void SigSep_tick(void);

#endif /* #ifndef __SIGNAL_SEPARATOR_H__ */
