# 14_adc_dma_2ch：观察两路采样怎样进入缓冲区

这个例程使用一个 ADC，按序列转换两个输入通道，再经 FIFO 和 DMA 把结果搬到内存。它适合学习通道序列、FIFO 打包和 DMA 传输长度之间的关系。

## 数据怎样走

```text
输入通道 0 ─┐
            ├→ ADC0 顺序转换 → FIFO → DMA → gADCSamples[2048]
输入通道 1 ─┘
```

[empty.syscfg](empty.syscfg) 中，ADC0 使用序列转换模式，包含 MEM0 和 MEM1。保存的引脚解为 PA27 和 PA26。

[Hardware/adc.c](Hardware/adc.c) 分配了 2048 个 `uint16_t` 样本。FIFO 每次读取打包两个 16 位结果，因此 DMA 设置为 1024 次 FIFO 读取。理解这两种计数单位后，修改采集长度会直观得多。

两路由同一个 ADC 依次转换，采样时刻有先后。用于功率、相位等测量时，可以继续研究这段时间差带来的影响，以及双 ADC 同一事件触发的实现方式。

## 在 CCS 中打开

在 CCS 的 **Import CCS Projects** 中，把搜索目录选到当前 `14_adc_dma_2ch` 文件夹。工程入口信息保存在 [.project](.project)、[.cproject](.cproject) 和 [.ccsproject](.ccsproject) 中。

保存的工程使用 TI Arm Clang **3.2.2 LTS**、MSPM0 SDK **2.01.00.03**，板级配置为 **LQFP-64(PM)** 和 **24 MHz HFXT**。共同环境、SysConfig 版本记录和导入步骤见[例程总览](../README.md)。先在 SysConfig 中对照自己的板卡，查看时钟树及 ADC 引脚分配。

## 第一次实验看什么

1. 阅读 [empty.c](empty.c)：`adc_init()` 开启 ADC 中断，`dma_init()` 设置 FIFO 源地址、缓冲区地址和传输长度。
2. 在两个通道分别施加容易区分、处于 ADC 量程内的信号，例如两个不同的直流电平。
3. 采集后在调试器中查看 `gADCSamples`，用输入差异确认两个通道在数组中的实际排列。
4. 在 `ADC0_IRQHandler()` 中观察 DMA 完成事件和 `check_dma_ADC` 标志。

主循环中的 OLED 显示仍读取 `adc_value`，而单次读取函数 `adc_get()` 已被注释。学习 DMA 时，以 `gADCSamples` 为主要观察对象。

## 关键源码

| 文件 | 先看什么 |
| --- | --- |
| [empty.c](empty.c) | 初始化与主循环 |
| [empty.syscfg](empty.syscfg) | ADC 序列、MEM0/MEM1、FIFO、DMA 和引脚 |
| [Hardware/adc.c](Hardware/adc.c) | `dma_init()`、样本长度和 `ADC0_IRQHandler()` |
| [Hardware/adc.h](Hardware/adc.h) | 对外变量与函数声明 |

## 接下来读什么

[15_adc_dma_time_trigger](../15_adc_dma_time_trigger/README.md) 进一步引入固定采样节拍；[12_FFT](../12_FFT/README.md) 展示一帧数据怎样进入频域分析。把两路用于电压、电流测量的设计思路见[2024 B题方案](../../2024/B题-单相功率分析仪-MSPM0G3507实现方案.md)。

来源：[Raoxiuyu/TI-MSPM0G3507-code](https://github.com/Raoxiuyu/TI-MSPM0G3507-code)。原 TI 模板说明保留在[整理前版本](https://github.com/liusuan110/2026-Ti/blob/baa2572bd9b8b3b52cd470acbf58d3a98b66d2b9/MSPM0%E4%BE%8B%E7%A8%8B-%E5%8F%8CADC%E5%90%8C%E6%AD%A5%E9%87%87%E6%A0%B7%E4%B8%8EFFT/14_adc_dma_2ch/README.md)。
