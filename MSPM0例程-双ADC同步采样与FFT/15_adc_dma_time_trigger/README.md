# 15_adc_dma_time_trigger：用定时器确定采样节拍

这个例程把“什么时候采样”交给定时器，再用 DMA 搬运结果。阅读时重点跟踪事件怎样从定时器传到 ADC，以及采集结果怎样进入缓冲区。

## 触发和数据分成两条线

```text
TIMA0 周期事件 ── 事件通道 1 ──→ ADC0 开始转换
                                  ↑
输入 PA27 ────────────────────────┘
                                  ↓
                              ADC FIFO
                                  ↓ DMA
                         gADCSamples[1024]
```

[empty.syscfg](empty.syscfg) 将 `TIMER_ADC` 的周期设为 **1 μs**，在零事件上发布到事件通道 1；ADC 的触发源选为事件，并订阅同一通道。配置的目标是让 ADC 按固定节拍接受转换触发。

[Hardware/adc.c](Hardware/adc.c) 分配 1024 个 `uint16_t` 样本。FIFO 一次打包两个 16 位结果，DMA 因此设置为 512 次 FIFO 读取。

## 在 CCS 中打开

在 CCS 的 **Import CCS Projects** 中，把搜索目录选到当前 `15_adc_dma_time_trigger` 文件夹。工程由 [.project](.project)、[.cproject](.cproject) 和 [.ccsproject](.ccsproject) 描述。

保存的工程使用 TI Arm Clang **3.2.2 LTS**、MSPM0 SDK **2.01.00.03**，板级配置为 **LQFP-64(PM)** 和 **24 MHz HFXT**。共同环境、SysConfig 版本记录和导入步骤见[例程总览](../README.md)。时钟决定定时器节拍，封装决定引脚映射；先在 `empty.syscfg` 中对照自己的板卡，再开始采样实验。

## 第一次实验看什么

1. 在 [empty.syscfg](empty.syscfg) 中依次找到 `TIMER_ADC`、事件通道和 ADC 触发源，把上图对应到配置项。
2. 在 [empty.c](empty.c) 中找到 TIMA0 启动与 `dma_init()`，再进入 [Hardware/adc.c](Hardware/adc.c) 查看源地址、目标地址和传输长度。
3. 给 PA27 输入处于 ADC 量程内的已知信号，在调试器中观察 `gADCSamples`。
4. 若用 DMA 完成中断观察一帧结束，同时查看 `ADC0_IRQHandler()` 和 `check_dma_ADC`。

当前 `main()` 中的 `adc_init()` 被注释，而 ADC 中断的 NVIC 使能位于这个函数中。要通过完成中断设置标志时，先恢复或补上这一步；原始缓冲区和中断标志分别帮助观察“数据有没有搬进来”和“程序有没有收到完成通知”。

主循环保留了 OLED、按键和串口演示。学习定时采样时，可以先把注意力放在事件路由与样本缓冲区。

## 关键源码

| 文件 | 先看什么 |
| --- | --- |
| [empty.syscfg](empty.syscfg) | `TIMER_ADC`、1 μs 周期、事件发布与订阅 |
| [empty.c](empty.c) | 定时器启动和外设初始化 |
| [Hardware/adc.c](Hardware/adc.c) | FIFO、DMA、缓冲区和 `ADC0_IRQHandler()` |

## 接下来读什么

[14_adc_dma_2ch](../14_adc_dma_2ch/README.md) 展示两路采样进入缓冲区的方法；[12_FFT](../12_FFT/README.md) 展示采样数据的频域处理。用于 H题时，固定节拍还会参与频率换算与相位控制，见[H题模块化工程](../../2023-H题-信号分离-番茄派工程/代码/README.md)。

来源：[Raoxiuyu/TI-MSPM0G3507-code](https://github.com/Raoxiuyu/TI-MSPM0G3507-code)。原 TI 模板说明保留在[整理前版本](https://github.com/liusuan110/2026-Ti/blob/baa2572bd9b8b3b52cd470acbf58d3a98b66d2b9/MSPM0%E4%BE%8B%E7%A8%8B-%E5%8F%8CADC%E5%90%8C%E6%AD%A5%E9%87%87%E6%A0%B7%E4%B8%8EFFT/15_adc_dma_time_trigger/README.md)。
