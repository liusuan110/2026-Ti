#include "SpectrumAnalyzer.h"

arm_rfft_instance_q15 RFFT2048_Instance = {
    .fftLenReal = 2048,
    .ifftFlagR = 0,
    .bitReverseFlagR = 1,
    .twidCoefRModifier = 4,
    .pTwiddleAReal = realCoefAQ15,
    .pTwiddleBReal = realCoefBQ15,
    .pCfft = &arm_cfft_sR_q15_len1024
};
q15_t FFTResult[FFT_SIZE * 2] = {0};

uint16_t SpectrumPeaksPos[SPECTRUM_N_PEAKS_MAX] = {0};

static void removeOffset(q15_t* data, q15_t offset, uint16_t len);
static void complexMag(q15_t* src, q15_t* dst, uint16_t numSamples);
static uint16_t getMaxPos(q15_t* data, uint16_t len);
static uint16_t findPeaks(q15_t* data, uint16_t len, uint16_t* peaksPos,
                          uint16_t nPeaksMax, q15_t minPeakHeight);

/**
 * @brief 计算幅度谱
 * @details 将时域数据减去偏置后, 计算幅度谱(使用2048点FFT)
 * @note 幅度谱将被存回data指向的数组
 * @param data 数据指针
 * @param offset 时域数据偏置
 */
void SA_getAmplitudeSpectrum(q15_t* data, q15_t offset) {
    removeOffset(data, offset, FFT_SIZE);
    arm_rfft_q15(&RFFT2048_Instance, data, FFTResult);
    complexMag(FFTResult, data, FFT_SIZE);
}

/**
 * @brief 寻找频谱幅度峰值
 * @details 在data数组中寻峰, 将峰值对应频谱数据存入spectrumPeaks数组
 * @param data 数据指针
 * @param fs 采样率(Sa/s)
 * @param spectrumPeaks 频谱数据存储结构体指针
 * @param nPeaksMax 最大峰值数(从第一个元素开始运算, 峰值数量达到nPeaksMax时终止)
 * @param minMag 最小峰值对应幅度
 * @return 找到的峰值数量
 */
uint16_t SA_getSpectrumPeaks(q15_t* data, float fs, SA_SpectrumData_t* spectrumPeaks,
                             uint16_t nPeaksMax, float minMag) {
    uint16_t spectrumPeaksN;
    uint16_t i;
    if (nPeaksMax > SPECTRUM_N_PEAKS_MAX) {
        nPeaksMax = SPECTRUM_N_PEAKS_MAX;
    }
    spectrumPeaksN = findPeaks(data, (FFT_SIZE >> 1), SpectrumPeaksPos, nPeaksMax, (q15_t)(minMag * 0.5F));
    for (i = 0; i < spectrumPeaksN; i++) {
        spectrumPeaks[i].freq = (float)SpectrumPeaksPos[i] * fs / FFT_SIZE;
        spectrumPeaks[i].mag = (float)data[SpectrumPeaksPos[i]] * 2.0F;
    }
    return spectrumPeaksN;
}

/**
 * @brief 按幅度从大到小排序频谱数据
 * @details 使用冒泡排序算法
 * @param data 频谱数据结构体指针
 * @param len 频谱数据长度
 */
void SA_sortSpectrumData(SA_SpectrumData_t* data, uint16_t len) {
    SA_SpectrumData_t temp;
    uint16_t i, j;
    for (i = 0; i < len; i++) {
        for (j = 1; j < (len - i); j++) {
            if (data[j].mag > data[j - 1].mag) {
                temp.freq = data[j].freq;
                temp.mag = data[j].mag;
                data[j].freq = data[j - 1].freq;
                data[j].mag = data[j - 1].mag;
                data[j - 1].freq = temp.freq;
                data[j - 1].mag = temp.mag;
            }
        }
    }
}

static void removeOffset(q15_t* data, q15_t offset, uint16_t len) {
    uint16_t i;
    for (i = 0; i < len; i++) {
        data[i] -= offset;
    }
}

static void complexMag(q15_t* src, q15_t* dst, uint16_t numSamples) {
    float32_t magSquared, mag;
    uint16_t i;
    for (i = 0; i < numSamples; i++) {
        magSquared = (float32_t)src[i * 2] * (float32_t)src[i * 2] +
                     (float32_t)src[i * 2 + 1] * (float32_t)src[i * 2 + 1];
        arm_sqrt_f32(magSquared, &mag);
        dst[i] = (q15_t)mag;
    }
}

static uint16_t getMaxPos(q15_t* data, uint16_t len) {
    q15_t max = data[0];
    uint16_t i, maxPos = 0;
    for (i = 1; i < len; i++) {
        if (max < data[i]) {
            max = data[i];
            maxPos = i;
        }
    }
    return maxPos;
}

/**
 * @brief 寻峰算法
 * @details 在data数组中寻峰, 将峰值对应下标存入peaksPos数组
 * @param data 数据指针
 * @param len 数据长度
 * @param peaksPos 峰值下标存储数组指针
 * @param nPeaksMax 最大峰值数(从第一个元素开始运算, 峰值数量达到nPeaksMax时终止)
 * @param minPeakHeight 最小峰值高度
 * @return 找到的峰值数量
 */
static uint16_t findPeaks(q15_t* data, uint16_t len, uint16_t* peaksPos,
                          uint16_t nPeaksMax, q15_t minPeakHeight) {
    uint8_t rising = 1;
    uint16_t startPos = 0, peaksN = 0;
    uint16_t i;
    for (i = 1; i < len; i++) {
        if (data[i] > data[i - 1]) {
            rising = 1;
            startPos = i;
        }
        else if (rising && (data[i] < data[i - 1])) {
            rising = 0;
            if (data[i - 1] >= minPeakHeight) {
                peaksPos[peaksN] = ((i - startPos) >> 1) + startPos;
                peaksN++;
                if (peaksN >= nPeaksMax) {
                    return peaksN;
                }
            }
        }
    }
    if (rising && (data[len - 1] >= minPeakHeight)) {
        peaksPos[peaksN] = ((i - startPos) >> 1) + startPos;
        peaksN++;
    }
    return peaksN;
}
