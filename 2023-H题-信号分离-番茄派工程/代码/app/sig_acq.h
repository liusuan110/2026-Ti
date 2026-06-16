#ifndef SIG_ACQ_H
#define SIG_ACQ_H

#include <stdint.h>
#include "h_config.h"

/* ADC 采集 (番茄派 ADC0, 12 位, 目标 fs=1.024MSa/s).
 * 当前为骨架: ADC0 单通道 + 软件触发轮询读取.
 * TODO(上板调试): 升级为 TIMG 事件硬件触发 + DMA 搬运, 才能稳定到 1.024MSa/s
 *   (侦察已给出确切 API: DL_ADC12_enableDMA / DL_DMA_* / TIMG pub-sub 触发). */

void     SigAcq_init(void);
uint16_t SigAcq_capture(uint16_t *buf, uint16_t n);   /* 采 n 点(12位 0..4095), 返回实际点数 */

#endif /* SIG_ACQ_H */
