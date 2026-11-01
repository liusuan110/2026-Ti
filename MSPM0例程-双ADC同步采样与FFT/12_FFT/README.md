# 12_FFT：把一帧采样变成频谱

这个例程展示一条最小分析链：ADC 采集 1024 个点，由 DMA 放进内存，随后去除直流分量，执行复数 FFT，得到各频点的模值。适合先在调试器中观察“原始采样—FFT 输入—频谱结果”三个阶段。

## 代码怎样走

```text
ADC0 / PA27
    ↓ FIFO + DMA
gADCSamples[1024]           原始 ADC 码值
    ↓ Create_data2handle()
data2handle[2048]           去直流后，实部与虚部交错存放
    ↓ arm_cfft_f32()
    ↓ arm_cmplx_mag_f32()
FFT_Output[1024]           频谱模值
```

[empty.c](empty.c) 完成初始化，在主循环中调用 `FFT_pro()`。采集、DMA 和 FFT 处理集中在 [Hardware/adc.c](Hardware/adc.c)；[empty.syscfg](empty.syscfg) 保存 ADC、时钟和引脚配置。

`FFT_pro()` 在首帧完成后关闭 ADC 转换和 DMA，再计算频谱。阅读这份代码时，可以先把目标定为观察一次完整结果；连续采集需要另行安排缓冲区重装和下一帧启动。

## 在 CCS 中打开

在 CCS 的 **Import CCS Projects** 中，把搜索目录选到当前 `12_FFT` 文件夹。工程由 [.project](.project)、[.cproject](.cproject) 和 [.ccsproject](.ccsproject) 描述；`empty.syscfg` 是配置文件。

保存的工程使用 TI Arm Clang **3.2.2 LTS**、MSPM0 SDK **2.01.00.03**，板级配置为 **LQFP-64(PM)** 和 **24 MHz HFXT**。共同环境、SysConfig 版本记录和导入步骤见[例程总览](../README.md)。在自己的开发板上运行时，先对照板卡确认封装、时钟和 PA27 输入路径。

## 第一次实验看什么

1. 从 `main()` 开始，观察 `dma_init()` 怎样把 FIFO 连接到 `gADCSamples`。
2. 给 PA27 输入处于 ADC 量程内的已知信号，在 `FFT_pro()` 中的 `Create_data2handle()` 之后、`arm_cfft_f32()` 调用之前暂停。
3. 查看 `gADCSamples` 的波形，再查看此时的 `data2handle`：偶数位置是去直流后的实部，奇数位置是零虚部。
4. 继续执行到 `arm_cmplx_mag_f32()` 之后，查看 `FFT_Output` 的正频率半谱，比较改变输入频率前后的峰值位置。`arm_cfft_f32()` 会原地改写 `data2handle`，因此输入与结果分别在这两个位置观察。

频谱下标 `k` 对应的频率为 `f = k × Fs / 1024`。这里的 `Fs` 应由本例程的采样配置与实际采样节拍确定。结果数组目前保存模值；转换成单边幅度谱、RMS 或 THD 时，再加入相应的归一化和谐波计算。

OLED 初始化和主循环中的固定文本属于外围演示。第一次实验直接观察内存数组，最容易看清 FFT 链路。

## 接下来读什么

- 看固定采样节拍：[15_adc_dma_time_trigger](../15_adc_dma_time_trigger/README.md)。
- 看两路数据进入内存：[14_adc_dma_2ch](../14_adc_dma_2ch/README.md)。
- 看频谱结果怎样参与识别和输出控制：[H题模块化工程](../../2023-H题-信号分离-番茄派工程/代码/README.md)。

来源：[Raoxiuyu/TI-MSPM0G3507-code](https://github.com/Raoxiuyu/TI-MSPM0G3507-code)。原 TI 模板说明保留在[整理前版本](https://github.com/liusuan110/2026-Ti/blob/baa2572bd9b8b3b52cd470acbf58d3a98b66d2b9/MSPM0%E4%BE%8B%E7%A8%8B-%E5%8F%8CADC%E5%90%8C%E6%AD%A5%E9%87%87%E6%A0%B7%E4%B8%8EFFT/12_FFT/README.md)。
