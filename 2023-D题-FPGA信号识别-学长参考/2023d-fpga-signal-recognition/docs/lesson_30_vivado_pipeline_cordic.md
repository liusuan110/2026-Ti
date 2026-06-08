# 2023D 第三十课：用 Vivado 跑仿真，并把 CORDIC 换成流水线版

这一课开始，我们把工程从“只写文件和 Python 参考模型”推进到：

```text
Vivado / XSim 参与验证
```

同时，项目主线不再使用迭代版 CORDIC。

## 1. 为什么不继续用迭代版 CORDIC

迭代版 CORDIC 的特点是：

```text
一套旋转硬件重复用 20 次
```

它省资源，但是吞吐低：

```text
约 20 拍才能算完一个 phase
```

而我们的 DDC 输出 I/Q 是连续来的。后面做 FM 参数估计时，相位序列越完整越舒服，所以更适合：

```text
流水线 CORDIC
```

## 2. 流水线版是什么意思

流水线版 CORDIC 把 20 次旋转拆成 20 级：

```text
第 0 级旋转 atan(1)
第 1 级旋转 atan(1/2)
第 2 级旋转 atan(1/4)
...
第 19 级旋转 atan(1/2^19)
```

每一级都有寄存器。

所以它的特点是：

```text
延迟：20 拍
吞吐：每拍可以接收 1 组 I/Q
```

这句话很重要：

```text
延迟高，不等于速度慢。
```

流水线填满以后，它可以每拍输出一个 phase。

## 3. 新模块

文件：

```text
rtl/cordic_atan2_pipeline.v
```

接口比迭代版更简单：

```verilog
valid_in
i_in
q_in

valid_out
phase_out
```

它没有：

```text
input_ready
busy
```

因为流水线版默认每拍都能接收新输入。

## 4. 用 Vivado 跑这一课

把第 30 课文件加入 Vivado GUI 工程：

```powershell
.\scripts\run_lesson_30_add_to_vivado_project.ps1
```

先生成测试数据：

```powershell
python .\scripts\lesson_30_prepare_and_check_pipeline_cordic.py
```

然后用 Vivado/XSim 仿真：

```powershell
.\scripts\run_lesson_30_vivado_xsim.ps1
```

这个脚本会自动调用：

```text
D:\Xilinx\Vivado\2023.1\bin\vivado.bat
```

如果以后 Vivado 装在别的位置，就改：

```text
scripts/run_lesson_30_vivado_xsim.ps1
```

## 5. 输出文件

Python 期望结果：

```text
outputs/lesson_30/cordic_atan2_pipeline_expected.csv
outputs/lesson_30/cordic_atan2_pipeline_expected.png
```

Vivado/XSim 仿真结果：

```text
outputs/lesson_30/cordic_atan2_pipeline_sim.csv
```

最后脚本会比较：

```text
Python 参考模型
Vivado/XSim Verilog 输出
```

如果一致，说明流水线 CORDIC 的 RTL 行为和 Python 模型对上了。

## 6. 这一课要记住

迭代版：

```text
省资源
吞吐低
需要 ready/busy
```

流水线版：

```text
多用一些寄存器和加法器
每拍可以接收输入
更适合连续信号处理
```

我们后续项目主线使用：

```text
cordic_atan2_pipeline
```

## 7. 小检查题

### 题 1

为什么流水线 CORDIC 有 20 拍延迟，但仍然可以说它“吞吐高”？

### 题 2

为什么流水线版不需要 `busy`？

### 题 3

如果输入 I/Q 每一拍都有效，迭代版和流水线版哪个更适合？为什么？
