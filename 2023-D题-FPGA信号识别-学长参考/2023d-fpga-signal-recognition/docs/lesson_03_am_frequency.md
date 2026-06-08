# 2023D 第三课：AM 调制频率 F 估计

上一课我们从 AM 信号里估计了包络和调幅系数 `m_a`。

这一课继续做基本要求（4）里 AM 分支的另一个参数：

```text
正弦调制信号频率 F
```

题目把 `F` 限定在 5 个候选值里：

```text
1 kHz, 2 kHz, 3 kHz, 4 kHz, 5 kHz
```

所以我们不需要先做一个很复杂的通用频率计。当前可以做“候选频率匹配”。

## 1. 已经有什么

从第二课得到：

```text
ADC 采样 x[n]
  -> abs(x[n])
  -> moving average
  -> envelope_est[n]
```

如果输入是 AM，则 `envelope_est[n]` 应该近似：

```text
E[n] = DC + A * sin(2*pi*F*n/Fs)
```

这里：

```text
DC：包络的直流中心，大约是 1
A ：包络变化幅度，和 m_a 有关
F ：我们要估计的调制频率
```

## 2. 为什么先减平均值

包络里有一个很大的直流分量：

```text
E[n] = 1 + m_a * sin(...)
```

如果直接拿它去匹配频率，那个 `1` 会干扰判断。

所以先做：

```text
env_ac[n] = envelope_est[n] - mean(envelope_est)
```

`ac` 是 alternating component 的意思，可以理解为去掉直流后的“交流变化部分”。

## 3. 候选频率匹配

对每个候选频率 `f_k`，都生成两条参考波：

```text
cos(2*pi*f_k*t)
sin(2*pi*f_k*t)
```

然后计算：

```text
I = sum(env_ac * cos_ref)
Q = sum(env_ac * sin_ref)
score = sqrt(I^2 + Q^2)
```

为什么要同时用 sin 和 cos？

因为真实包络不一定刚好从正弦的 0 相位开始。

如果只用 sin，遇到相位差 90 度的同频信号，分数会很低。加上 cos 后，相当于不在乎初相，只判断“是不是这个频率”。

最后选分数最大的候选频率：

```text
estimated_F = argmax(score)
```

## 4. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_03_am_frequency_estimator.py
```

输出文件：

```text
outputs/lesson_03/am_frequency_estimator.png
outputs/lesson_03/am_frequency_estimator.csv
```

## 5. 这是不是 FFT

不是完整 FFT。

它更像只检查 5 个频点的简化频谱分析。

后面如果搬到 FPGA，可以有几种实现：

```text
1. Goertzel 算法，只算 5 个频点
2. 用 5 组 sin/cos 查表做相关
3. 用过零周期估计，先做简单版
```

现在 Python 版选择相关法，是因为它概念清楚，也能直接解释“为什么 3 kHz 得分最高”。

## 6. 小变形检查题

### 题 1

如果 `F = 5 kHz`，包络周期是多少？

提示：

```text
T = 1 / F
```

### 题 2

为什么候选匹配前要先减平均值？

### 题 3

为什么每个候选频率同时要和 `sin`、`cos` 两条参考波比较，而不是只和 `sin` 比较？
