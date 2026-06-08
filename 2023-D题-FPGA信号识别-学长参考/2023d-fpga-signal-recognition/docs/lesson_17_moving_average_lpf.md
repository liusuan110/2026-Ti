# 2023D 第十七课：移动平均低通滤波器

上一课 Mixer 输出：

```text
I_raw = x * cos
Q_raw = x * -sin
```

但是这还不是真正干净的基带 I/Q。

因为乘法会产生两类分量：

```text
低频基带分量
高频 2fc 附近分量
```

所以这一课要接：

```text
低通滤波器 LPF
```

教学版先用最简单的：

```text
移动平均 Moving Average
```

## 1. 为什么混频后要低通

以最简单的同频 cos 为例：

```text
x(t) = A cos(2*pi*fc*t)
```

I 路混频：

```text
x(t) * cos(2*pi*fc*t)
```

用积化和差：

```text
cos(a)cos(a) = 1/2 + 1/2 cos(2a)
```

所以：

```text
I_raw = 低频常数 + 高频 2fc 分量
```

Q 路：

```text
x(t) * -sin(2*pi*fc*t)
```

会得到一个高频摆动项。对于同相输入，低频 Q 应该接近 0。

所以低通后：

```text
I -> 接近常数
Q -> 接近 0
```

## 2. 移动平均公式

窗口长度是 `N`：

```text
y[n] = (x[n] + x[n-1] + ... + x[n-N+1]) / N
```

这一课取：

```text
N = 16
```

因为 16 是 2 的幂：

```text
除以 16 = 右移 4 位
```

硬件实现比较简单。

## 3. 硬件里不能每次重新加 16 个数

直接写：

```text
每个时钟把最近 16 个数全部加一遍
```

当然能理解，但硬件资源浪费。

更好的写法是维护一个运行和：

```text
sum_next = sum_old + 新样本 - 最旧样本
avg = sum_next >>> 4
```

也就是：

```text
进来一个新数
踢掉一个旧数
更新一次总和
```

这就是 `rtl/moving_average.v` 的核心。

## 4. 为什么输出一开始 invalid

窗口长度是 16。

刚复位后，延迟线里还没有 16 个真实采样。

所以前 15 个输出不可信：

```text
valid_out = 0
```

等第 16 个采样进入后：

```text
valid_out = 1
```

这就是为什么 FPGA 里经常有：

```text
valid 信号
```

它告诉后级模块：

```text
这一拍的数据能不能用。
```

## 5. Verilog 模块

文件：

```text
rtl/moving_average.v
```

核心逻辑：

```verilog
next_sum = sum_acc + data_in - oldest_sample;
avg_out  = next_sum >>> SHIFT_BITS;
```

其中：

```text
>>> 是算术右移
```

它会保留符号位，适合 signed 数据。

## 6. 位宽

输入是 Mixer 的输出：

```text
21 bit signed
```

16 点累加最多会多出：

```text
log2(16) = 4 bit
```

所以累加器用：

```text
21 + 4 = 25 bit
```

这就是：

```verilog
ACC_BITS = DATA_BITS + SHIFT_BITS
```

## 7. 准备脚本

文件：

```text
scripts/lesson_17_prepare_and_check_lpf.py
```

它会生成：

```text
sim/lpf_i_raw_80.mem
sim/lpf_q_raw_80.mem
outputs/lesson_17/moving_average_expected.csv
outputs/lesson_17/moving_average_expected.png
```

运行：

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_17_prepare_and_check_lpf.py
```

## 8. 仿真文件

文件：

```text
sim/tb_moving_average.v
```

它会实例化两个移动平均模块：

```text
一个处理 I_raw
一个处理 Q_raw
```

如果以后装好了 Icarus Verilog，可以运行：

```powershell
iverilog -o outputs\lesson_17\tb_moving_average.vvp rtl\moving_average.v sim\tb_moving_average.v
vvp outputs\lesson_17\tb_moving_average.vvp
python .\scripts\lesson_17_prepare_and_check_lpf.py
```

最后一个 Python 命令会比较 Verilog 仿真输出和 Python 期望输出。

## 9. 这一课要记住

```text
Mixer 负责搬频。
LPF 负责把搬频后不想要的高频项滤掉。
```

移动平均的硬件核心是：

```text
加新样本，减旧样本，维护运行和。
```

## 10. 小变形检查题

### 题 1

为什么窗口长度 16 时，平均值可以用右移 4 位实现？

### 题 2

为什么前 15 个输出不能立刻认为有效？

### 题 3

为什么低通后，同相测试信号的 Q 路应该接近 0？
