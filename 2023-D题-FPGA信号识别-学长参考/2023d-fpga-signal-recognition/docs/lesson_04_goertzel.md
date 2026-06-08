# 2023D 第四课：从相关法理解 Goertzel

先记住一句话：

```text
Goertzel 算法 = 只计算某几个指定频点的能量
```

它不是魔法，也不是必须先学完 FFT 才能用。

第三课的相关法已经在做这件事：

```text
看看包络更像 1k / 2k / 3k / 4k / 5k 里的哪一个
```

Goertzel 只是换了一种更适合硬件递推的写法。

## 1. 第三课相关法回顾

对一个候选频率 `f`：

```text
I = sum(x[n] * cos(2*pi*f*n/Fs))
Q = sum(x[n] * sin(2*pi*f*n/Fs))
score = sqrt(I^2 + Q^2)
```

它的意思是：

```text
如果 x[n] 里真的有这个频率，score 会大
如果没有这个频率，score 会小
```

这已经是“频点检测”。

## 2. Goertzel 的递推形式

对某个目标频率，Goertzel 用：

```text
s[n] = x[n] + coeff * s[n-1] - s[n-2]
```

其中：

```text
coeff = 2 * cos(2*pi*f/Fs)
```

处理完一段长度为 `N` 的数据后，算：

```text
power = s[N-2]^2 + s[N-1]^2 - coeff * s[N-1] * s[N-2]
```

这个 `power` 就是这个目标频率的能量分数。

## 3. 它和相关法有什么关系

相关法是显式地乘：

```text
sin reference
cos reference
```

Goertzel 是用递推状态 `s[n]` 间接完成类似的频点检测。

可以先这样理解：

```text
相关法：直接拿尺子去量这个频率像不像
Goertzel：做了一个只对这个频率特别敏感的递推器
```

结果上，它们都能回答：

```text
这个候选频率有没有出现？
```

## 4. 为什么适合 2023D

题目里 AM/FM 的调制频率只可能是：

```text
1 kHz, 2 kHz, 3 kHz, 4 kHz, 5 kHz
```

所以不需要完整 FFT。

只需要 5 个频点检测器：

```text
Goertzel(1k)
Goertzel(2k)
Goertzel(3k)
Goertzel(4k)
Goertzel(5k)
```

哪个 `power` 最大，就选哪个频率。

## 5. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_04_goertzel_from_correlation.py
```

输出图：

```text
outputs/lesson_04/goertzel_from_correlation.png
```

这份脚本会同时输出：

```text
相关法得分
Goertzel 得分
```

你会看到它们选中的候选频率一致。

## 6. 现在先不用证明

这一课先不推导 z 变换，也不推导 DFT。

你只需要掌握：

```text
相关法和 Goertzel 都是候选频点打分
相关法更直观
Goertzel 更适合少数频点检测
题目只有 5 个候选频率，所以 Goertzel 很合适
```

后面真正搬到 FPGA 前，我们再处理：

```text
定点数
coeff 系数量化
溢出位宽
窗口长度 N
```
