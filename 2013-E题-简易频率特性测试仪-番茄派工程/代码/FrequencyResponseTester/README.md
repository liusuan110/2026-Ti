# FrequencyResponseTester

2013 E题《简易频率特性测试仪》的番茄派 MSPM0G3507 CCS 空工程。

## 当前状态

工程仅保留可编译、可继续配置的最小启动骨架：

- `FrequencyResponseTester.c`：初始化 SysConfig 后进入空循环
- `FrequencyResponseTester.syscfg`：仅声明 MSPM0G3507、LQFP-48 (PT) 与基础系统模块
- `targetConfigs/MSPM0G3507.ccxml`：J-Link 调试目标配置
- `.project`、`.cproject`、`.ccsproject`：CCS 工程元数据
- `FrequencyResponseTester.projectspec`：必要时可从模板重新导入工程

未配置 GPIO、ADC、DMA、定时器、串口、显示、DDS 或任何测量算法。

## 环境

- Code Composer Studio 20.2.0
- MSPM0 SDK 2.10.00.04
- SysConfig 1.26.2
- TI Arm Clang 4.0.3.LTS

在 CCS 中导入本目录即可继续开发。若使用 XDS110 而不是 J-Link，请在 CCS 中切换连接配置。
