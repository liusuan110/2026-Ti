# 2023D 第三十三课：接出主链路 DDC -> CORDIC -> unwrap

前面三课分别做了：

```text
第 31 课：DDC -> CORDIC -> wrapped phase
第 32 课：wrapped phase -> unwrapped phase
```

这一课把它们接成主链路：

```text
ADC sample -> DDC -> I/Q -> pipeline CORDIC -> wrapped phase -> unwrap -> unwrapped phase
```

## 1. 新模块

文件：

```text
rtl/ddc_phase_unwrap_core.v
```

它内部实例化：

```text
ddc_phase_core_pipeline
phase_unwrap
```

输出：

```text
phase_unwrapped_valid
phase_unwrapped_out
```

调试输出：

```text
phase_sample_index_debug
i_phase_debug
q_phase_debug
phase_wrapped_debug
wrap_offset_debug
```

## 2. 为什么调试信号要重新对齐

`phase_unwrap` 接在 CORDIC 后面。

它会让 phase 再经过一级寄存器。

所以如果直接拿 CORDIC 那一拍的 I/Q 和 index，会错一拍。

这一课的 wrapper 做了对齐：

```text
unwrapped phase
wrapped phase debug
I/Q debug
sample index debug
```

都对应同一组样本。

## 3. 为什么测试用 5000 Hz 频偏

这节测试输入是：

```text
2 MHz + 5000 Hz 的 CW
```

这不是说题目一定要这么大频偏。

它只是为了让 phase 在短仿真里多次跨过：

```text
+pi / -pi
```

这样才能真正测试 unwrap 是否工作。

## 4. 用 Vivado 跑这一课

跑仿真：

```powershell
.\scripts\run_lesson_33_vivado_xsim.ps1
```

加入 Vivado GUI 工程：

```powershell
.\scripts\run_lesson_33_add_to_vivado_project.ps1
```

## 5. 输出文件

```text
outputs/lesson_33/ddc_phase_unwrap_core_expected.csv
outputs/lesson_33/ddc_phase_unwrap_core_sim.csv
outputs/lesson_33/ddc_phase_unwrap_core_expected.png
```

脚本会比较：

```text
Python 参考模型
Vivado/XSim RTL 输出
```

## 6. 这一课要记住

从这一课开始，FM 分支的相位输入应该看：

```text
phase_unwrapped_out
```

而不是：

```text
phase_out
```

因为 `phase_out` 仍然被限制在 `-pi~+pi`，跨界时会跳变。

## 7. 小检查题

### 题 1

为什么第 33 课要故意用比较大的 5000 Hz 频偏做测试？

### 题 2

为什么 `phase_wrapped_debug` 和 `phase_unwrapped_out` 必须对齐到同一组样本？

### 题 3

后面 FM 分支应该用 wrapped phase 还是 unwrapped phase？为什么？
