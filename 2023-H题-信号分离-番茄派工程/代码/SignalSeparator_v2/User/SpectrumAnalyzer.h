#ifndef __SPECTRUM_ANALYZER_H__
#define __SPECTRUM_ANALYZER_H__

#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "arm_const_structs.h"
#include "arm_math.h"

#define FFT_SIZE 2048
#define SPECTRUM_N_PEAKS_MAX 5

// 频谱数据
typedef struct SA_SpectrumData_t {
    float freq; // 频率
    float mag;  // 幅值
} SA_SpectrumData_t;

/**
 * @brief 计算幅度谱
 * @details 将时域数据减去偏置后, 计算幅度谱(使用2048点FFT)
 * @note 幅度谱将被存回data指向的数组
 * @param data 数据指针
 * @param offset 时域数据偏置
 */
void SA_getAmplitudeSpectrum(q15_t* data, q15_t offset);

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
                             uint16_t nPeaksMax, float minMag);

/**
 * @brief 按幅度从大到小排序频谱数据
 * @details 使用冒泡排序算法
 * @param data 频谱数据结构体指针
 * @param len 频谱数据长度
 */
void SA_sortSpectrumData(SA_SpectrumData_t* data, uint16_t len);

#endif /* #ifndef __SPECTRUM_ANALYZER_H__ */
