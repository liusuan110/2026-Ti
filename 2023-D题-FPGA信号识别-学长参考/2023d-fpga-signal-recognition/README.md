# 2023D FPGA Signal Recognition Demo

这是一个面向全国大学生电子设计竞赛 2023 年 D 题方向的 FPGA 学习与验证工程。当前重点是用低成本 FPGA + 高速 AD/DA 模块搭出一个可复现实验平台，逐步实现调制方式识别与参数估计。

## 当前进度

已在 `ACX720-V3 35T + ACM2108` 上跑通上板环回演示：

```text
DAC0 -> ADC0 SMA 环回
FPGA 内部生成并识别 6 类信号：
CW / AM / FM / 2ASK / 2PSK / 2FSK
LED 显示发送模式和识别结果
```

LED 编码：

```text
D5-D3: 发送模式
D2-D0: 识别结果

000 -> 000  CW
001 -> 001  AM
010 -> 010  FM
011 -> 100  2ASK
100 -> 101  2PSK
101 -> 110  2FSK
```

当前版本是“板上演示闭环”，不是完整比赛成品。后续目标包括外部信号输入、参数显示、解调输出、模拟前端和更稳健的判决策略。

## 硬件

已验证硬件：

```text
FPGA: ACX720-V3 35T, Artix-7 XC7A35T
AD/DA: ACM2108, 8-bit dual-channel ADC/DAC
连接: DAC0 SMA -> ADC0 SMA
```

约束文件：

```text
constraints/acx720_acm2108_receiver_loopback.xdc
```

## 目录

```text
rtl/          Verilog RTL
sim/          Testbench
scripts/      Python 数据生成脚本和 Vivado/XSim 批处理脚本
constraints/  板卡约束
docs/         学习笔记和设计说明
```

公开仓库不包含 Vivado 生成目录、bitstream、仿真波形、原题 PDF 提取全文或商家资料。请从官方渠道获取赛题原文。

## 快速验证

需要：

```text
Vivado 2023.1
PowerShell
Python 3
```

运行顶层环回仿真：

```powershell
.\scripts\run_acx720_acm2108_receiver_loopback_xsim.ps1
```

运行接收核心仿真：

```powershell
.\scripts\run_receiver_stream_core_xsim.ps1
```

综合：

```powershell
.\scripts\run_acx720_acm2108_receiver_loopback_synth.ps1
```

实现并生成 bitstream：

```powershell
.\scripts\run_acx720_acm2108_receiver_loopback_impl.ps1
```

生成的 bitstream 位于：

```text
outputs/acx720_acm2108_receiver_loopback_impl/acx720_acm2108_receiver_loopback_top.bit
```

`outputs/` 不纳入版本控制。

## 已知限制

- 35T 资源很紧，当前完整演示版本 LUT 使用率约 93%，不适合继续无节制叠加 ILA、屏幕和复杂算法。
- 当前演示默认内部生成信号并通过 DAC0 -> ADC0 环回识别。
- 外部信号输入、模拟前端保护、幅度适配和仪器测量流程仍在推进。
- 这是学习和备赛工程，算法会继续迭代，不保证所有边界条件都已覆盖。

## 适合怎么参与

如果你想一起学习或组队，可以先做三件事：

1. 跑通至少一个仿真脚本。
2. 阅读 `rtl/acx720_acm2108_receiver_loopback_top.v` 和 `rtl/receiver_stream_core.v`，理解数据流。
3. 提一个具体问题、复现实验结果，或者给出一个明确改进点。

欢迎通信原理、模拟电路、仪器测量、FPGA 时序优化、数字信号处理方向的同学一起完善。

## License

本仓库代码暂按 MIT License 开源。赛题原文、商家资料和第三方文档不属于本仓库授权范围。
