# 2023D 第三十一课：把 DDC Phase Core 换成流水线 CORDIC

上一课我们单独验证了：

```text
cordic_atan2_pipeline
```

这一课把它接回主线：

```text
ADC sample -> DDC Core -> I/Q -> Pipeline CORDIC -> phase
```

## 1. 为什么要替换第 29 课版本

第 29 课的结构是：

```text
DDC Core -> 迭代版 CORDIC
```

它必须等 CORDIC 空闲才能送下一组 I/Q。

所以中间很多 I/Q 会被跳过。

这一课改成：

```text
DDC Core -> 流水线 CORDIC
```

DDC 每输出一组有效 I/Q，就直接送进 CORDIC。

## 2. 新模块

文件：

```text
rtl/ddc_phase_core_pipeline.v
```

它实例化两个模块：

```text
ddc_core
cordic_atan2_pipeline
```

这次没有：

```text
cordic_ready
cordic_busy
```

因为流水线 CORDIC 默认每拍可以接收新输入。

## 3. 延迟对齐

流水线 CORDIC 有 20 拍延迟。

也就是说：

```text
第 n 组 I/Q 输入
20 拍后才得到第 n 组 phase
```

所以模块里不只延迟 `phase`，还要把调试信息一起延迟：

```text
phase_sample_index_debug
i_phase_debug
q_phase_debug
```

否则 phase 出来的时候，I/Q debug 已经变成后面的样本了。

## 4. 用 Vivado 跑这一课

把第 31 课文件加入 Vivado GUI 工程：

```powershell
.\scripts\run_lesson_31_add_to_vivado_project.ps1
```

跑 Vivado/XSim 仿真：

```powershell
.\scripts\run_lesson_31_vivado_xsim.ps1
```

这个脚本会做三件事：

```text
生成 Python 参考数据
用 Vivado/XSim 跑 Verilog 仿真
比较 Python 和 Verilog 输出
```

## 5. 输出文件

Python 期望结果：

```text
outputs/lesson_31/ddc_phase_core_pipeline_expected.csv
outputs/lesson_31/ddc_phase_core_pipeline_expected.png
```

Vivado/XSim 仿真结果：

```text
outputs/lesson_31/ddc_phase_core_pipeline_sim.csv
```

## 6. 这一课要记住

第 31 课比第 29 课更接近真实项目主线：

```text
第 29 课：能理解，但会跳过很多 I/Q
第 31 课：更费资源，但每个有效 I/Q 都能进入相位计算
```

后续 FM 参数估计会基于第 31 课版本继续往下走。

## 7. 小检查题

### 题 1

为什么 `phase_sample_index_debug` 也要延迟 20 拍？

### 题 2

第 31 课为什么不再需要 `cordic_ready` 和 `cordic_busy`？

### 题 3

如果 phase 输出晚 20 拍，会不会影响我们估计 FM 的调制频率？为什么？
