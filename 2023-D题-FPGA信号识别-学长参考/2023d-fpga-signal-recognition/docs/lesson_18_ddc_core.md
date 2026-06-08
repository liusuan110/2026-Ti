# 2023D 第十八课：把 DDS、Mixer、LPF 串成 DDC Core

前面三块已经有了：

```text
DDS/NCO            生成 cos/sin
Mixer              x*cos, x*(-sin)
Moving Average     低通滤波
```

这一课把它们串起来，得到一个完整的：

```text
DDC Core
```

DDC 是：

```text
Digital Down Converter
数字下变频器
```

## 1. DDC Core 做什么

输入：

```text
8 bit ADC 采样 x[n]
```

输出：

```text
低通后的 I_baseband
低通后的 Q_baseband
```

模块路径：

```text
sample_in
  ↓
DDS 生成 cos/sin
  ↓
Mixer: I_raw = x*cos, Q_raw = x*(-sin)
  ↓
Moving Average LPF
  ↓
I_baseband, Q_baseband
```

## 2. 为什么要注意 valid 对齐

这节课最重要的工程点不是公式，而是：

```text
sample、cos/sin、valid 必须对齐。
```

在 Verilog 里，DDS 输出 `cos/sin` 是寄存器输出。

也就是说：

```text
输入 valid_in 的这一拍
DDS 在时钟沿后才更新 cos/sin
```

所以 `cos/sin` 比 `sample_in` 晚一拍可用。

因此在 `ddc_core` 里要做：

```verilog
sample_d1 <= sample_in;
```

把采样值也延迟一拍。

这样下一拍：

```text
sample_d1 和 DDS 输出的 cos/sin 就对应同一个 n
```

## 3. ddc_core 模块

文件：

```text
rtl/ddc_core.v
```

主要实例化三个模块：

```verilog
dds_nco
ddc_mixer
moving_average for I
moving_average for Q
```

输出：

```verilog
valid_out
i_baseband
q_baseband
```

其中 `valid_out` 来自两个低通模块：

```verilog
assign valid_out = i_lpf_valid & q_lpf_valid;
```

也就是说：

```text
只有 I/Q 两路低通结果都有效时，输出才有效。
```

## 4. 为什么输出会晚一些

DDC Core 里有多级寄存器：

```text
DDS 输出寄存器
Mixer 输出寄存器
LPF 窗口填充
```

所以输入第一个采样后，不会立刻得到有效 I/Q。

这不是错误，而是流水线延迟。

FPGA 设计里经常出现：

```text
数据晚几拍出来
valid 信号跟着数据一起走
```

只要 `valid_out` 对，就没问题。

## 5. 这次测试什么信号

测试输入仍然是：

```text
x[n] = 80 * cos(2*pi*2MHz*n/20MHz)
```

也就是一个同相的 2 MHz cos。

理论上低通后：

```text
I_baseband 比较大
Q_baseband 接近 0
```

脚本结果：

```text
Mean I_baseband ≈ 82154.60
Mean Q_baseband ≈ 149.60
```

这说明：

```text
DDC Core 的方向是对的。
```

Q 不是严格 0，是因为当前低通只是 16 点移动平均。

## 6. 准备脚本

文件：

```text
scripts/lesson_18_prepare_and_check_ddc_core.py
```

它会生成：

```text
sim/ddc_core_sample_80.mem
outputs/lesson_18/ddc_core_expected.csv
outputs/lesson_18/ddc_core_expected.png
```

运行：

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_18_prepare_and_check_ddc_core.py
```

## 7. 仿真文件

文件：

```text
sim/tb_ddc_core.v
```

如果以后装好了 Icarus Verilog，可以运行：

```powershell
iverilog -o outputs\lesson_18\tb_ddc_core.vvp rtl\dds_nco.v rtl\ddc_mixer.v rtl\moving_average.v rtl\ddc_core.v sim\tb_ddc_core.v
vvp outputs\lesson_18\tb_ddc_core.vvp
python .\scripts\lesson_18_prepare_and_check_ddc_core.py
```

最后一步会检查 Verilog 仿真输出是否和 Python 期望一致。

## 8. 这一课要记住

```text
DDC Core = DDS + Mixer + LPF
```

但真正写硬件时，最容易出错的是：

```text
不是数学公式
而是每一级输出晚几拍
valid 和数据有没有对齐
```

所以 FPGA 里要养成习惯：

```text
每个模块都带 valid
每一级延迟都明确
```

## 9. 小变形检查题

### 题 1

为什么 `ddc_core` 里要把 `sample_in` 延迟一拍得到 `sample_d1`？

### 题 2

为什么 DDC Core 的输出不是输入第一拍后立刻有效？

### 题 3

如果输入是同相的 2 MHz cos，为什么低通后的 I 路大、Q 路接近 0？
