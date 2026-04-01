## Plan: MSP430 Vpp, Frequency & Drawing Optimization

**TL;DR** 
在严格不修改底层 LCD 驱动的前提下，通过纯算法层面的优化（展开循环、ISR 提速、跳过空白和未变化区域的绘图）来榨干MCU性能，提升采样频率和视觉刷新率。

**Steps**
1. **优化 Vpp 内层循环 (`ADC_measureVpp`)**：当前由于 `VPP_TOPK` 为 3，维持 Top-K 和 Bottom-K 的 `for` 循环完全可以手动展开为直接的 `if-else` 比较。这能大幅缩短每次 ADC 转换间的 CPU 空转占用，增强高频采样的连续性。
2. **优化频率捕获滤波 (`period_median`)**：`timer_capture.c` 的中断里用插入排序来做中值滤波，会长时间阻塞。我们将它替换为完全展开的极简 5 元素中值判断或去极值平均，极大地减少 ISR 占用时间，保障高频捕获稳定。
3. **消除冗余绘图通信 (`LCD_drawBars`)**：在 `lcd_draw.c` 中，绘制 FFT 时无差别遍历了全部 128 列及 6 个 page。我们将修改为根据 `mag` 数组，跳过完全不需要高度渲染的连续空白部分，显著减少对慢速软件 SPI 的调用。
4. **波形页渲染层级优化 (`page_wave`)**：调整 `display.c` 中的波形显示逻辑，进一步减少文字清屏和冗余擦除的通信开销。

**Relevant files**
- adc_measure.c — 展开 `Top-K` 排序机制，降低单次ADC采样后的处理延迟。
- timer_capture.c — 替换 `period_median`，减轻中断执行负担。
- lcd_draw.c — 重构 `LCD_drawBars` 降低操作开销。

**Verification**
1. 进行对比测试：通过信号发生器分别输入 1kHz 和 50kHz 的波形，确认测量结果不仅同样精准，并且抖动更少。
2. 观察 FFT 和 WAVE 页面刷新表现，由于砍掉了不必要的像素刷新请求计算量，应能明显感觉到帧率改善及闪烁感降低。

**Decisions**
- **明确排除**：无论及、也不修改任何底层 LCD 驱动(`JLX12864G.c`) 及相关配置。
