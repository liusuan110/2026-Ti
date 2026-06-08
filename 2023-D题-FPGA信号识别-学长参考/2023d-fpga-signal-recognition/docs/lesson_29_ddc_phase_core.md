# 2023D 第二十九课：把 DDC Core 接到 CORDIC，得到 phase

现在我们已经有：

```text
DDC Core -> I/Q
CORDIC atan2 -> phase
```

这一课把它们接起来：

```text
ADC sample -> DDC Core -> I/Q -> CORDIC -> phase
```

## 1. 为什么不能直接每拍都送 CORDIC

我们的 `cordic_atan2_iterative` 是迭代复用版。

它的特点是：

```text
一套硬件
一个样本大约 20 拍算完
```

而 `ddc_core` 在稳定后几乎每个有效采样时钟都会输出一组 I/Q。

所以如果直接每拍都送：

```text
CORDIC 处理不过来
```

这就是为什么 CORDIC 有：

```text
input_ready
busy
valid_out
```

这些握手信号。

## 2. 教学版怎么接

教学版只在 CORDIC 空闲时接收一组 I/Q：

```text
cordic_accept = ddc_valid && cordic_ready
```

如果 CORDIC 正在忙：

```text
跳过中间的 I/Q 样本
```

这相当于对 phase 进行抽样。

因为 AM/FM 的调制频率只有：

```text
1 kHz 到 5 kHz
```

即使 phase 样本少一些，教学验证仍然够用。

但如果以后要每个 ADC 样本都算 phase，就要换：

```text
流水线版 CORDIC
```

## 3. 新模块

文件：

```text
rtl/ddc_phase_core.v
```

内部结构：

```text
ddc_core
  ↓
cordic_atan2_iterative
```

输出：

```text
phase_valid
phase_out
```

还有几个调试信号：

```text
ddc_valid_debug
cordic_ready_debug
cordic_busy_debug
phase_sample_index_debug
i_phase_debug
q_phase_debug
```

其中：

```text
phase_sample_index_debug
```

表示这一组 phase 对应的是第几个 DDC 有效 I/Q 样本。

## 4. 为什么要保存 I/Q debug

CORDIC 输入 I/Q 后，要过很多拍才输出 phase。

如果不保存当时送进去的 I/Q，等 phase 出来的时候，DDC 的 I/Q 早就变成后面的样本了。

所以模块里在 `cordic_accept` 时保存：

```text
i_phase_debug <= i_baseband
q_phase_debug <= q_baseband
phase_sample_index_debug <= ddc_sample_index
```

这样 `phase_valid` 时，调试输出仍然能对应当初那组 I/Q。

## 5. 测试脚本

文件：

```text
scripts/lesson_29_prepare_and_check_phase_core.py
```

它生成一个测试输入：

```text
CW，初始相位 30 度，载波偏移 +200 Hz
```

这样 phase 应该是：

```text
初始相位 + 频偏造成的缓慢斜坡
```

脚本会生成：

```text
sim/phase_core_sample_8000.mem
outputs/lesson_29/ddc_phase_core_expected.csv
outputs/lesson_29/ddc_phase_core_expected.png
```

## 6. 仿真文件

文件：

```text
sim/tb_ddc_phase_core.v
```

如果以后装好了 Icarus Verilog，可以运行：

```powershell
iverilog -o outputs\lesson_29\tb_ddc_phase_core.vvp rtl\dds_nco.v rtl\ddc_mixer.v rtl\moving_average.v rtl\ddc_core.v rtl\cordic_atan2_iterative.v rtl\ddc_phase_core.v sim\tb_ddc_phase_core.v
vvp outputs\lesson_29\tb_ddc_phase_core.vvp
python .\scripts\lesson_29_prepare_and_check_phase_core.py
```

最后一步会比较 Verilog 仿真输出和 Python 期望输出。

## 7. 这一课要记住

```text
算法模块接起来以后，速度是否匹配很重要。
```

`ddc_core` 很快：

```text
几乎每拍一个 I/Q
```

迭代 CORDIC 较慢：

```text
约 20 拍一个 phase
```

所以教学版先用：

```text
ready/valid 握手 + 抽样
```

来保证数据不会乱。

## 8. 小变形检查题

### 题 1

为什么 `ddc_phase_core` 不能在 CORDIC 忙的时候继续把新 I/Q 塞进去？

### 题 2

为什么 `phase_valid` 输出时，还要保存当初送入 CORDIC 的 I/Q 调试值？

### 题 3

如果以后真的需要每个 DDC 样本都算 phase，应该考虑迭代版 CORDIC 还是流水线版 CORDIC？
