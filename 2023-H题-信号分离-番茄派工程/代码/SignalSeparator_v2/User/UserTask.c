#include "UserTask.h"

SigSep_Param_t SigSep_Param = {0};
SigSep_Data_t SigSep_Data = {0};

void UserTask_init(void) {
    ENC_init();
    SigSep_init(&SigSep_Param, &SigSep_Data);
}

void UserTask_loop(void) {
    SigSep_taskSpectrumAnalysis(&SigSep_Param, &SigSep_Data);
    SigSep_taskPLL(&SigSep_Param, &SigSep_Data);
    SigSep_taskPhaseShift();
    RGBLED_taskRainbow();
}
