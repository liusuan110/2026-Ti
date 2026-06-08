# 2023D 第六课：FM 参数估计

这一课对应 2023D 基本要求（2）和（4）里的 FM 分支：

```text
估计调频系数 m_f
估计最大频偏 Delta_f_max
估计调制频率 F
输出解调信号 u_o
```

我们先做参数估计。

## 1. FM 信号模型

单音 FM：

```text
x_FM(t) = cos(2*pi*f_c*t + m_f * sin(2*pi*F*t))
```

其中：

```text
f_c = 2 MHz
F   = 调制信号频率
m_f = 调频系数
```

调制相位是：

```text
phase_mod(t) = m_f * sin(2*pi*F*t)
```

所以相位摆动幅度就是：

```text
m_f
```

## 2. 先 DDC 得到相位

上一课讲过：

```text
FM 经过 DDC 后：
I ≈ cos(phase_mod)
Q ≈ sin(phase_mod)
```

所以：

```text
phase_est = atan2(Q, I) ≈ phase_mod
```

这一步把“2 MHz 高频问题”变成了“低频相位曲线问题”。

## 3. 估计 m_f

如果：

```text
phase_mod(t) = m_f * sin(2*pi*F*t)
```

那么：

```text
phase_max = +m_f
phase_min = -m_f
```

因此：

```text
m_f = (phase_max - phase_min) / 2
```

如果相位里有一个常数偏移，先减平均值即可。

## 4. 估计 F

`F` 仍然只可能是：

```text
1 kHz, 2 kHz, 3 kHz, 4 kHz, 5 kHz
```

所以可以复用第三课的候选频率匹配：

```text
phase_ac = phase_est - mean(phase_est)
```

然后对 5 个候选频率打分。

## 5. 估计 Delta_f_max

FM 的瞬时频率是：

```text
f_inst(t) = f_c + m_f * F * cos(2*pi*F*t)
```

所以最大频偏：

```text
Delta_f_max = m_f * F
```

单位注意：

```text
m_f 无单位
F 单位是 Hz
Delta_f_max 单位是 Hz
```

例如：

```text
m_f = 3
F = 5 kHz
Delta_f_max = 15 kHz
```

## 6. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_06_fm_parameter_estimator.py
```

输出：

```text
outputs/lesson_06/fm_parameter_estimator.png
outputs/lesson_06/fm_parameter_estimator.csv
```

## 7. 小变形检查题

### 题 1

如果：

```text
m_f = 4
F = 2 kHz
```

最大频偏 `Delta_f_max` 是多少？

### 题 2

如果估计到相位曲线：

```text
phase_max = 2.5 rad
phase_min = -2.5 rad
```

那么 `m_f` 是多少？

### 题 3

为什么 FM 不应该用 AM 那种 `sqrt(I^2 + Q^2)` 的幅度变化去估计参数？
