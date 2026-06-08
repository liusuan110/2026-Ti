# 2023D 第十六课：Mixer 混频模块

上一课 DDS/NCO 已经能输出：

```text
cos[n]
sin[n]
```

这一课做 DDC 的第一步：

```text
Mixer 混频
```

## 1. Mixer 要做什么

DDC 的目标是把 2 MHz 附近的输入信号搬到低频。

输入是 ADC 采样：

```text
x[n]
```

DDS 给出本地振荡器：

```text
cos[n]
sin[n]
```

Mixer 做：

```text
I_raw[n] = x[n] * cos[n]
Q_raw[n] = x[n] * (-sin[n])
```

这就是第 5 课 Python 里的：

```python
lo_i = 2.0 * cos(2*pi*fc*t)
lo_q = -2.0 * sin(2*pi*fc*t)
i_baseband = lowpass(x * lo_i)
q_baseband = lowpass(x * lo_q)
```

这一课只做乘法，还不做低通。

## 2. 为什么 Q 路是负的 sin

我们希望把输入信号乘上：

```text
e^(-j 2*pi*fc*t)
```

它等价于：

```text
cos(2*pi*fc*t) - j sin(2*pi*fc*t)
```

所以：

```text
I 路用 cos
Q 路用 -sin
```

这就是：

```text
I_raw = x * cos
Q_raw = x * -sin
```

符号约定有不同写法，但我们后面的 `atan2(Q, I)` 会按这个约定走。

## 3. 位宽为什么会变大

这道题里现在先按：

```text
ADC sample: 8 bit signed
DDS cos/sin: 12 bit signed
```

有符号乘法的结果大约需要：

```text
8 + 12 = 20 bit
```

但 Q 路要先把 `sin` 取负。

如果 `sin` 是最小负数，直接取负可能溢出。虽然我们的 sin 表刻意没有用 `-2048`，但写硬件时稳一点：

```text
先把 sin 扩展成 13 bit
再取负
```

所以模块输出用了：

```text
PRODUCT_BITS = 8 + 12 + 1 = 21 bit
```

这多出来的 1 bit 是为了让符号和取负更稳。

## 4. Verilog 模块

文件：

```text
rtl/ddc_mixer.v
```

核心是：

```verilog
assign i_product = sample_in * cos_in;
assign neg_sin_ext = -$signed({sin_in[LO_BITS-1], sin_in});
assign q_product = sample_in * neg_sin_ext;
```

这里：

```text
{sin_in[LO_BITS-1], sin_in}
```

是在做符号扩展。

比如 `sin_in` 是负数，它的最高位是 1，扩展后仍然是负数。

## 5. 为什么这一课还不右移缩放

现在：

```text
sample_in 是 8 bit
cos_in 是 12 bit
i_raw 是 21 bit
```

乘法结果会很大。

后面低通滤波还会累加更多点，位宽会继续变大。

所以这一课先保留完整乘法结果：

```text
先不截断
先不缩放
先不右移
```

等低通滤波后，再统一考虑缩放和位宽。

这样更不容易一开始就把有用信息截掉。

## 6. 准备脚本

文件：

```text
scripts/lesson_16_prepare_and_check_mixer.py
```

它会生成 testbench 需要的输入：

```text
sim/mixer_sample_80.mem
sim/mixer_cos_80.mem
sim/mixer_sin_80.mem
```

也会生成 Python 期望输出：

```text
outputs/lesson_16/ddc_mixer_expected.csv
outputs/lesson_16/ddc_mixer_expected.png
```

运行：

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_16_prepare_and_check_mixer.py
```

## 7. 仿真文件

文件：

```text
sim/tb_ddc_mixer.v
```

它会读取三份 `.mem`：

```text
sample
cos
sin
```

把它们送进 `ddc_mixer`，再把输出写到：

```text
outputs/lesson_16/ddc_mixer_sim.csv
```

如果以后装好了 Icarus Verilog，可以运行：

```powershell
iverilog -o outputs\lesson_16\tb_ddc_mixer.vvp rtl\ddc_mixer.v sim\tb_ddc_mixer.v
vvp outputs\lesson_16\tb_ddc_mixer.vvp
python .\scripts\lesson_16_prepare_and_check_mixer.py
```

最后一步会比较 Verilog 仿真输出和 Python 期望输出。

## 8. 这一课要记住

```text
Mixer 本质上就是两个有符号乘法器。
```

但写 FPGA 时要特别注意：

```text
输入是不是 signed
乘法结果要多少位
取负会不会溢出
valid 信号是否对齐
```

## 9. 小变形检查题

### 题 1

如果 ADC 是 8 bit，DDS 是 12 bit，为什么乘法结果不能还只用 12 bit 保存？

### 题 2

为什么 Q 路用的是 `-sin`，不是 `sin`？

### 题 3

这一课为什么先不对乘法结果右移缩放？
