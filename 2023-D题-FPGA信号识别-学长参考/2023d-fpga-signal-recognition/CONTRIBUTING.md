# Contributing

这个项目优先欢迎“能复现、能提具体问题、愿意一起把系统做完整”的贡献。

## 建议参与方式

1. 先跑通一个仿真脚本，例如：

```powershell
.\scripts\run_acx720_acm2108_receiver_loopback_xsim.ps1
```

2. 如果有 ACX720 + ACM2108，可以尝试生成 bitstream 并上板：

```powershell
.\scripts\run_acx720_acm2108_receiver_loopback_synth.ps1
.\scripts\run_acx720_acm2108_receiver_loopback_impl.ps1
```

3. 反馈时请尽量包含：

```text
硬件型号
Vivado 版本
运行的脚本
观察到的现象
你认为可能的问题点
```

## 当前优先级

- 外部输入链路与幅度适配
- 参数显示方案
- AM/FM/CW 与 2ASK/2PSK/2FSK 判决稳定性
- 资源优化，尤其是 35T 版本
- 测试报告和演示材料

## 不建议的参与方式

- 只问“能不能发完整工程”，但不说明自己想做哪部分。
- 直接贴大段资料，不给结论或问题。
- 不跑仿真、不看 README，就要求远程排错。

这个项目还在快速迭代，具体、可复现的反馈最有价值。
