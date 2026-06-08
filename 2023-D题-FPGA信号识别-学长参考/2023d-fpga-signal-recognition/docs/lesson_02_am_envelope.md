# 2023D 第二课：AM 包络检测与调幅系数估计

这一课对应 2023D 基本要求（1）：

```text
已知 u_M 为 AM 信号
正弦调制信号频率 F = 1 kHz
估计并显示调幅系数 m_a
输出解调信号 u_o
```

先做最小版本：

```text
Python 生成 AM 采样
模拟 8 位 ADC 量化
从采样里估计包络
由包络最大值/最小值估计 m_a
```

## 1. AM 的关键公式

普通 AM：

```text
x(t) = A_c * [1 + m_a * sin(2*pi*F*t)] * cos(2*pi*f_c*t)
```

中括号这一项就是包络：

```text
E(t) = 1 + m_a * sin(2*pi*F*t)
```

如果 `m_a = 0.6`：

```text
E_max = 1 + 0.6 = 1.6
E_min = 1 - 0.6 = 0.4
```

反过来，只要能估计出包络最大值和最小值，就能算：

```text
m_a = (E_max - E_min) / (E_max + E_min)
```

这个公式很重要。它不依赖绝对幅度，所以前端放大倍数不准也没关系，只要不削顶。

## 2. FPGA 友好的包络检测

这一课先用最简单的算法：

```text
ADC 采样 x[n]
  -> 取绝对值 abs(x[n])
  -> 移动平均，滤掉整流后的高频纹波
  -> 得到包络估计值 E_est[n]
```

为什么可以这样？

AM 的载波很快：

```text
f_c = 2 MHz
```

包络很慢：

```text
F = 1 kHz
```

取绝对值之后，原来的正负半周会被翻到正半轴，主要纹波频率变成 `2*f_c = 4 MHz`，还会有更高次谐波。

只要做一个低通/移动平均，就能把这些很快的纹波压下去，保留慢的包络变化。

## 3. 为什么要乘 pi/2

代码里有一句：

```python
envelope_est = envelope_est * (np.pi / 2)
```

原因是：

```text
abs(cos) 的平均值 = 2/pi
```

移动平均后得到的是：

```text
包络 * 2/pi
```

所以乘回：

```text
pi/2
```

让估计包络的数值接近真实包络。

即使不乘 `pi/2`，`m_a` 的计算也基本不受影响，因为最大值和最小值会同时缩放；但乘上后更方便画图对齐。

## 4. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_02_am_envelope_estimator.py
```

输出文件：

```text
outputs/lesson_02/am_envelope_estimator.png
outputs/lesson_02/am_envelope_estimator.csv
```

## 5. 小变形检查题

这些不是背诵题，重点是分清“真实信号”和“算法特征”。

### 题 1

如果 `m_a = 0.3`，包络最大值和最小值分别是多少？

提示：

```text
E_max = 1 + m_a
E_min = 1 - m_a
```

### 题 2

如果某次估计得到：

```text
E_max = 2.4
E_min = 0.6
```

那么 `m_a` 是多少？

提示：

```text
m_a = (E_max - E_min) / (E_max + E_min)
```

### 题 3

为什么 `E_max = 2.4`、`E_min = 0.6` 和 `E_max = 1.6`、`E_min = 0.4` 算出来的 `m_a` 一样？

这题对应真实硬件里的前端放大器：放大倍数变了，但调幅系数不应该变。
