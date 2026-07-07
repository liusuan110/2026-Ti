#include "SignalSeparator.h"

q15_t MagData[FFT_SIZE] = {0};
SA_SpectrumData_t SpectrumPeaks[SPECTRUM_N_PEAKS_MAX] = {0};
uint16_t SpectrumPeaksN = 0;
DDS_SingleToneParam_t DDS_SingleTone[4] = {0};
PID_Param_t PID_Param_50kHz = {0};
PID_Param_t PID_ParamA = {0};
PID_Param_t PID_ParamB = {0};
PID_Data_t PID_DataA = {0};
PID_Data_t PID_DataB = {0};

volatile uint16_t SigSep_SATick = 0; // 频谱分析任务计时变量
volatile uint16_t SigSep_PLLTick = 0; // DDS任务计时变量
volatile uint16_t SigSep_PhaseShiftTick = 0; // 相位差任务计时变量

static void getSigData(SigSep_Data_t* data, SA_SpectrumData_t* spectrumPeaks, uint16_t spectrumPeaksN);
static void setOutputSigType(SigSep_Data_t* data);
static void getPhaseError(SigSep_Param_t* param, SigSep_Data_t* data);

void SigSep_init(SigSep_Param_t* param, SigSep_Data_t* data) {
    DDS_init();
    UserADC_init();

    param->fs = 1024000;
    param->sigOffset = 2048;
    param->PDOffset = 1.67F;
    data->sigA.type = SIG_TYPE_SINE;
    data->sigB.type = SIG_TYPE_SINE;

    UserADC_setEventTimer(param->fs);
    UserADC0_startDMA((uint16_t*)MagData, FFT_SIZE);

    DDS_SingleTone[0].freq = 0.0F;
    DDS_SingleTone[0].amp = 1023;
    DDS_SingleTone[0].phase = 0x0000;
    DDS_SingleTone[1].freq = 0.0F;
    DDS_SingleTone[1].amp = 1023;
    DDS_SingleTone[1].phase = 0x0000;
    DDS_SingleTone[2].freq = 0.0F;
    DDS_SingleTone[2].amp = 1023;
    DDS_SingleTone[2].phase = 0x1000;
    DDS_SingleTone[3].freq = 0.0F;
    DDS_SingleTone[3].amp = 1023;
    DDS_SingleTone[3].phase = 0x1000;

    PID_Param_50kHz.kp = 2.0F;
    PID_Param_50kHz.ki = 1.0F;
    PID_Param_50kHz.kd = 0.0F;
    PID_ParamA.intMax = 10.0F;
    PID_ParamA.outMax = 5.0F;
    PID_ParamB.intMax = 10.0F;
    PID_ParamB.outMax = 5.0F;
    PID_DataA.target = 0.0F;
    PID_DataB.target = 0.0F;
}

void SigSep_taskSpectrumAnalysis(SigSep_Param_t* param, SigSep_Data_t* data) {
    if (SigSep_SATick >= SIG_SEP_SA_TIME) {
        SigSep_SATick = 0;
        if (IS_ADC0_DONE) {
            DL_GPIO_setPins(GPIO_TEST_PORT, GPIO_TEST_SA_PIN);
            SA_getAmplitudeSpectrum(MagData, param->sigOffset);
            SpectrumPeaksN = SA_getSpectrumPeaks(MagData, (float)(param->fs), SpectrumPeaks,
                                                SPECTRUM_N_PEAKS_MAX, 20.0F);
            SA_sortSpectrumData(SpectrumPeaks, SpectrumPeaksN);
            getSigData(data, SpectrumPeaks, SpectrumPeaksN);
            setOutputSigType(data);
            DL_GPIO_clearPins(GPIO_TEST_PORT, GPIO_TEST_SA_PIN);
            UserADC0_startDMA((uint16_t*)MagData, FFT_SIZE);
        }
    }
}

void SigSep_taskPLL(SigSep_Param_t* param, SigSep_Data_t* data) {
    float freqPLLA, freqPLLB, paramFactor;
    if (SigSep_PLLTick >= SIG_SEP_PLL_TIME) {
        SigSep_PLLTick = 0;
        getPhaseError(param, data);
        PID_DataA.feedback = data->basebandA;
        PID_DataB.feedback = data->basebandB;
        paramFactor = data->sigA.freq * 20E-6F;
        PID_ParamA.kp = PID_Param_50kHz.kp * paramFactor;
        PID_ParamA.ki = PID_Param_50kHz.ki * paramFactor;
        PID_ParamA.kd = PID_Param_50kHz.kd * paramFactor;
        paramFactor = data->sigB.freq * 20E-6F;
        PID_ParamB.kp = PID_Param_50kHz.kp * paramFactor;
        PID_ParamB.ki = PID_Param_50kHz.ki * paramFactor;
        PID_ParamB.kd = PID_Param_50kHz.kd * paramFactor;
        PID_getOutput(&PID_ParamA, &PID_DataA);
        PID_getOutput(&PID_ParamB, &PID_DataB);
        freqPLLA = data->sigA.freq + PID_DataA.out;
        freqPLLB = data->sigB.freq + PID_DataB.out;
        DDS_SingleTone[0].freq = freqPLLA;
        DDS_SingleTone[1].freq = freqPLLB;
        DDS_SingleTone[2].freq = freqPLLA;
        DDS_SingleTone[3].freq = freqPLLB;
        DDS_singleTone(AD9959_CH0, &DDS_SingleTone[0]);
        DDS_singleTone(AD9959_CH1, &DDS_SingleTone[1]);
        DDS_singleTone(AD9959_CH2, &DDS_SingleTone[2]);
        DDS_singleTone(AD9959_CH3, &DDS_SingleTone[3]);
        DDS_update();
    }
}

void SigSep_taskPhaseShift(void) {
    static int phaseShift = 0;
    if (SigSep_PhaseShiftTick >= SIG_SEP_PHASE_SHIFT_TIME) {
        SigSep_PhaseShiftTick = 0;
        phaseShift += ENC_getIncVal() * 5;
        DDS_SingleTone[3].phase = (uint16_t)(DDS_SingleTone[2].phase + (float)phaseShift * 45.51F);
    }
}

void SigSep_tick(void) {
    SigSep_SATick++;
    SigSep_PLLTick++;
    SigSep_PhaseShiftTick++;
}

static void getSigData(SigSep_Data_t* data, SA_SpectrumData_t* spectrumPeaks, uint16_t spectrumPeaksN) {
    data->sigA.type = SIG_TYPE_SINE;
    data->sigB.type = SIG_TYPE_SINE;
    if (spectrumPeaksN >= 2) {
        if (spectrumPeaks[0].freq <= spectrumPeaks[1].freq) {
            data->sigA.freq = spectrumPeaks[0].freq;
            data->sigB.freq = spectrumPeaks[1].freq;
        }
        else {
            data->sigA.freq = spectrumPeaks[1].freq;
            data->sigB.freq = spectrumPeaks[0].freq;
        }
        if (spectrumPeaksN > 2) {
            if (ABS(data->sigA.freq * 3.0F - spectrumPeaks[2].freq) < 500.5F) {
                data->sigA.type = SIG_TYPE_TRIANGLE;
            }
            else if (ABS(data->sigB.freq * 3.0F - spectrumPeaks[2].freq) < 500.5F) {
                data->sigB.type = SIG_TYPE_TRIANGLE;
            }
        }
        else if ((ABS(spectrumPeaks[0].freq * 3.0F - spectrumPeaks[1].freq) < 500.5F) &&
                 (spectrumPeaks[1].mag * 2.0F < spectrumPeaks[0].mag)) {
            data->sigA.freq = spectrumPeaks[0].freq;
            data->sigB.freq = spectrumPeaks[0].freq;
            data->sigA.type = SIG_TYPE_TRIANGLE;
        }
    }
    else {
        data->sigA.freq = spectrumPeaks[0].freq;
        data->sigB.freq = spectrumPeaks[0].freq;
    }
}

static void setOutputSigType(SigSep_Data_t* data) {
    if (data->sigA.type == SIG_TYPE_SINE) {
        RLY_A_OFF;
    }
    else {
        RLY_A_ON;
    }
    if (data->sigB.type == SIG_TYPE_SINE) {
        RLY_B_OFF;
    }
    else {
        RLY_B_ON;
    }
}

static void getPhaseError(SigSep_Param_t* param, SigSep_Data_t* data) {
    UserADC1_start();
    data->basebandA = UserADC1_getVoltage(0, 1) - param->PDOffset;
    data->basebandB = UserADC1_getVoltage(1, 1) - param->PDOffset;
}
