# 2023D 第十三课：DDS/NCO 为什么能产生 2 MHz

这一课讲一个 FPGA 里非常核心的模块：

```text
DDS / NCO
```

DDS 是：

```text
Direct Digital Synthesizer
直接数字频率合成器
```

NCO 是：

```text
Numerically Controlled Oscillator
数控振荡器
```

在我们这道题里，它的作用是：

```text
生成 DDC 用的 cos(2*pi*fc*t) 和 sin(2*pi*fc*t)
```

## 1. 先把正弦波看成“绕圈”

正弦/余弦不是凭空跳出来的，它可以看成一个点绕圆转：

```text
转到 0 度      cos = 1,  sin = 0
转到 90 度     cos = 0,  sin = 1
转到 180 度    cos = -1, sin = 0
转到 270 度    cos = 0,  sin = -1
```

所以产生正弦波的关键不是先算 sin/cos，而是先知道：

```text
现在转到圆上的哪个相位位置？
```

## 2. 相位累加器

FPGA 里用一个寄存器保存当前相位：

```text
phase
```

每来一个采样时钟，就加一个固定值：

```text
phase <= phase + phase_step
```

这就是相位累加器。

如果 `phase_step` 大，phase 每个时钟跳得远，转圈快，频率就高。

如果 `phase_step` 小，phase 每个时钟跳得近，转圈慢，频率就低。

## 3. phase_step 怎么算

假设：

```text
采样时钟 FS = 20 MHz
目标输出 FC = 2 MHz
相位寄存器 PHASE_BITS = 32 bit
```

32 bit 相位寄存器一整圈是：

```text
2^32
```

每秒有 `FS` 次更新。要每秒转 `FC` 圈，所以每次应该前进：

```text
phase_step = FC / FS * 2^PHASE_BITS
```

代入：

```text
phase_step = 2 MHz / 20 MHz * 2^32
           = 0.1 * 2^32
           ≈ 429496730
```

这就是以后 Verilog 里最重要的参数之一。

## 4. 为什么会自动循环

相位寄存器是有限位宽，比如 32 bit。

当它超过最大值，会自然回绕：

```text
0, 1, 2, ..., 2^32 - 1, 0, 1, 2, ...
```

这正好对应圆周相位：

```text
0 到 2*pi，再回到 0
```

所以“溢出回绕”在 DDS 里不是错误，而是我们想要的效果。

## 5. 查表 ROM

有了 phase，还要得到 cos/sin。

FPGA 不会每个时钟实时计算：

```text
cos(phase)
sin(phase)
```

而是提前存一张表：

```text
地址 0      -> cos/sin 在 0 度的值
地址 256    -> cos/sin 在 90 度的值
地址 512    -> cos/sin 在 180 度的值
地址 768    -> cos/sin 在 270 度的值
```

如果表有 1024 个点，就用 phase 的高 10 位当地址：

```text
lut_addr = phase[31:22]
```

然后：

```text
cos_value = cos_rom[lut_addr]
sin_value = sin_rom[lut_addr]
```

## 6. 为什么用高位当地址

phase 的高位代表“大位置”，低位代表“很细的小数部分”。

查表 ROM 点数有限，比如只有 1024 点，不可能给 32 bit phase 的每个值都存一个 cos。

所以用高位决定当前在哪个大格子：

```text
phase 高 10 位 -> 1024 个表地址
phase 低 22 位 -> 被截掉，带来很小的量化误差
```

这就是 DDS 里的相位截断。

## 7. 频率精度从哪里来

真正输出频率是：

```text
actual_freq = phase_step / 2^PHASE_BITS * FS
```

因为 `phase_step` 必须是整数，所以可能有一点点频率误差。

但是 32 bit 很细，误差通常非常小。

本课脚本里：

```text
target FC = 2000000 Hz
phase_step = 429496730
actual_freq = 2000000.001863 Hz
freq_error = 0.001863 Hz
```

对 2023D 的 2 MHz 载波来说，这个误差几乎可以忽略。

## 8. 和 DDC 的关系

DDC 需要本地振荡器：

```text
cos(2*pi*fc*t)
-sin(2*pi*fc*t)
```

DDS/NCO 就是负责生成它们的模块。

然后 Mixer 做：

```text
I_raw = x * cos
Q_raw = x * (-sin)
```

所以 DDS 是 DDC 的前置基础。

## 9. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_13_dds_nco_demo.py
```

输出：

```text
outputs/lesson_13/dds_nco_samples.csv
outputs/lesson_13/dds_nco_demo.png
```

## 10. 这一课要记住

```text
DDS 的本质不是“算正弦”。
DDS 的本质是“每个时钟让相位前进一步，再用相位查表得到正弦/余弦”。
```

最核心的一行是：

```text
phase <= phase + phase_step
```

最核心的公式是：

```text
phase_step = FC / FS * 2^PHASE_BITS
```

## 11. 小变形检查题

### 题 1

如果 `phase_step` 变大，输出频率会变高还是变低？

### 题 2

为什么相位寄存器溢出回到 0，在 DDS 里不是错误？

### 题 3

如果 `FS = 20 MHz`，`FC = 1 MHz`，`PHASE_BITS = 32`，那么：

```text
phase_step ≈ ?
```

不用算到个位，先给出大概数量级也可以。
