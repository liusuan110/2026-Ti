# TI 电赛资料仓库索引

本仓库用于存放电赛信号类、仪器仪表类资料、模块手册和当前 2023 H 题工程。

## 当前主线

- 培训4，2013 E题番茄派 CCS 空工程：
  `2013-E题-简易频率特性测试仪-番茄派工程/代码/FrequencyResponseTester`
- 当前准备继续推进的工程：
  `2023-H题-信号分离-番茄派工程/代码/SignalSeparator_v2`
- 旧 H 工程备份/阶段工程：
  `2023-H题-信号分离-番茄派工程/代码`
- AD9959 模块资料：
  `AD9959-DDS信号源资料`
  `培训资料/康威科技_AD9959模块驱动+PDF-V2.6`
- 串口屏资料：
  `2023-H题-信号分离-番茄派工程/串口屏`
  `培训资料/tjcwiki.pdf`

## 当前注意事项

- `SignalSeparator_v2` 的 AD9959 接线与旧 H 工程不同。
- `SignalSeparator_v2` 的主输入 C 为 `ADC0 PA27`。
- 仓库不保存 CCS/Keil 生成物，例如 `Debug/`、`build/`、`.clangd`、`.theia/`、`*.theia-workspace`、`*.out`、`*.o`、`*.d`。
- 厂商资料中的压缩包本地保留，仓库主要保存解压后的手册、例程和说明。

## 建议工作流

1. 修改工程前先看 `git status --short`。
2. CCS 编译通过后只提交源文件、`.syscfg`、`.project/.cproject/.ccsproject`、说明文档。
3. 烧录失败优先检查调试器连接、芯片型号和 Flash 容量，不先怀疑算法。
4. 每晚结束前在 H 题工程目录补一条状态记录。
