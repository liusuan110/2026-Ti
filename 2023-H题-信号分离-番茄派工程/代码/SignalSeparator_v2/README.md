# SignalSeparator_v2 - 2023 H题信号分离当前主工程

本目录是当前准备继续使用的 CCS / MSPM0G3507 工程，来源于 `D:\desktop\SignalSeparator_v2\SignalSeparator_v2`，已整理进 TI 仓库。

## 当前状态

- 目标芯片：MSPM0G3507
- SDK：MSPM0 SDK 2.10.00.04
- 编译器：TI ARM LLVM 4.0.2 LTS
- SysConfig：1.26.2
- 已修复：
  - CMSIS-DSP include 路径从旧 SDK 修到当前 SDK。
  - SysConfig 新增 `@v2CliArgs --device "MSPM0G3507"`，避免生成成 `MSPM0G3505`。
  - clean 后重新生成的 linker 为 `FLASH=0x20000`、`SRAM=0x8000`。
- 已验证：
  - `gmake clean && gmake all` 可生成 `Debug\SignalSeparator_v2.out`。

## AD9959 接线

本工程的 AD9959 接线不同于旧 H 工程：

| AD9959 信号 | MSPM0G3507 引脚 |
|---|---|
| SCLK | PA8 |
| SDIO0 | PA9 |
| CS / CSB | PB3 |
| IO_UPDATE / UPDATE | PB2 |
| RST | PA7 |

仅接必要控制线、电源和共地。不要把 `PDC/PWR_DWN/OUT/SYNC` 等非必要引脚直接接地。

## ADC 输入

- 主输入 C：`ADC0 PA27`
- ADC1：`PA16 / PA17`，用于基带/鉴相信号。

## AD9959 输出逻辑

程序不是上电固定输出 50 kHz / 100 kHz。默认流程是：

1. ADC0 采集输入 C。
2. FFT 识别两路频率。
3. PLL/PID 计算修正频率。
4. AD9959 输出四路：
   - CH0：A 路输出，频率 `freqPLLA`，相位 0 度。
   - CH1：B 路输出，频率 `freqPLLB`，相位 0 度。
   - CH2：A 路同频正交参考，频率 `freqPLLA`，相位 90 度。
   - CH3：B 路同频正交参考，频率 `freqPLLB`，相位 90 度加手动偏移。

## 明日优先测试顺序

1. 先确认芯片丝印是否为 MSPM0G3507。
2. 解决 J-Link / CCS 烧录连接问题。
3. 临时加入 DDS 固定输出自测，确认 AD9959 CH0/CH1 有波形。
4. 再接 `ADC0 PA27`，验证单频输入能被识别。
5. 最后测双频混合输入和 CH0/CH1 跟随输出。
