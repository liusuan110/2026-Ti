# 2023D 第五课：数字下变频 DDC 与 I/Q

这一课讲一个后面会反复用到的工具：

```text
DDC = Digital Down Conversion = 数字下变频
```

它的作用是：

```text
把 2 MHz 载波搬到 0 Hz 附近
```

这样我们就不用一直盯着很快的 2 MHz 波形，而是观察慢得多的：

```text
幅度变化
相位变化
频率变化
```

## 1. 为什么要下变频

ADC 直接看到的是：

```text
x[n] = 某个慢变化特征 * 2 MHz 载波
```

对于 AM：

```text
x_AM(t) = E(t) * cos(2*pi*f_c*t)
```

真正有用的是慢变化的包络 `E(t)`。

对于 FM：

```text
x_FM(t) = cos(2*pi*f_c*t + phase_mod(t))
```

真正有用的是慢变化的相位 `phase_mod(t)`，再由相位变化得到瞬时频率。

DDC 就是把高频载波去掉，留下这些慢变化特征。

## 2. I/Q 是什么

我们同时准备两条本地参考波：

```text
I 路： 2*cos(2*pi*f_c*t)
Q 路：-2*sin(2*pi*f_c*t)
```

然后：

```text
I_mixed = x(t) * 2*cos(2*pi*f_c*t)
Q_mixed = x(t) * -2*sin(2*pi*f_c*t)
```

再经过低通/移动平均：

```text
I = lowpass(I_mixed)
Q = lowpass(Q_mixed)
```

这两个量就是基带 I/Q。

## 3. AM 经过 DDC 后会怎样

AM 信号：

```text
x_AM(t) = E(t) * cos(2*pi*f_c*t)
```

理想下变频后：

```text
I ≈ E(t)
Q ≈ 0
```

所以：

```text
sqrt(I^2 + Q^2) ≈ E(t)
```

这说明 AM 包络可以通过 I/Q 幅度得到。

## 4. FM 经过 DDC 后会怎样

FM 信号：

```text
x_FM(t) = cos(2*pi*f_c*t + phase_mod(t))
```

理想下变频后：

```text
I ≈ cos(phase_mod(t))
Q ≈ sin(phase_mod(t))
```

所以：

```text
phase_est = atan2(Q, I) ≈ phase_mod(t)
```

FM 的调制信息藏在相位里。下一课会从这个相位变化估计：

```text
调频系数 m_f
最大频偏 Delta_f_max
```

## 5. CW 经过 DDC 后会怎样

CW 信号：

```text
x_CW(t) = cos(2*pi*f_c*t)
```

理想下变频后：

```text
I ≈ 1
Q ≈ 0
```

所以：

```text
幅度基本恒定
相位基本恒定
```

这就是 CW 和 AM/FM 的重要区别。

## 6. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_05_ddc_iq.py
```

输出：

```text
outputs/lesson_05/ddc_iq.png
outputs/lesson_05/ddc_iq.csv
```

## 7. 小变形检查题

### 题 1

如果输入是理想 AM：

```text
x(t) = E(t) * cos(2*pi*f_c*t)
```

DDC 后 `sqrt(I^2 + Q^2)` 近似等于什么？

### 题 2

如果输入是理想 FM：

```text
x(t) = cos(2*pi*f_c*t + phase_mod(t))
```

DDC 后 `atan2(Q, I)` 近似等于什么？

### 题 3

为什么 DDC 后还要低通/移动平均？

提示：乘法会产生一个基带分量，也会产生一个高频分量。
