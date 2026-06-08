# 2023D 第十课：频偏估计与相位去线性趋势

第九课发现一个关键问题：

```text
如果输入载波不是精确 2 MHz，而是 2 MHz + Delta_f，
DDC 后的相位会出现线性斜坡。
```

这会导致：

```text
CW 看起来像 FM
FM 的 m_f 被估得过大
```

这一课解决这个问题。

## 1. 为什么频偏会造成相位斜坡

假设真实输入是 CW：

```text
x(t) = cos(2*pi*(f_c + Delta_f)*t)
```

但 DDC 用的是：

```text
cos(2*pi*f_c*t)
sin(2*pi*f_c*t)
```

下变频后还会剩下：

```text
Delta_f
```

所以基带相位近似：

```text
phase(t) = 2*pi*Delta_f*t + const
```

这是一条直线。

斜率是：

```text
slope = 2*pi*Delta_f
```

所以：

```text
Delta_f = slope / (2*pi)
```

## 2. 什么叫去线性趋势

先拟合一条直线：

```text
phase_line(t) = slope * t + intercept
```

再从原相位里减掉它：

```text
phase_detrended(t) = phase(t) - phase_line(t)
```

这就叫 detrend，去线性趋势。

对于 CW：

```text
原 phase 是斜线
减掉斜线后接近 0
```

对于 FM：

```text
原 phase = 斜线 + 正弦摆动
减掉斜线后保留正弦摆动
```

## 3. 这对分类有什么帮助

原来：

```text
CW + 频偏 -> phase_span 很大 -> 可能误判成 FM
```

去趋势后：

```text
CW -> phase_span 变小
FM -> phase_span 仍然大
```

所以 CW / FM 更容易分开。

## 4. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_10_phase_detrend.py
```

输出：

```text
outputs/lesson_10/phase_detrend.png
```

## 5. 重要提醒

这节课用的是 Python 的：

```python
np.polyfit(t, phase, 1)
```

它很适合教学，但 FPGA 上不会直接这么写。

FPGA 里可以用更简单的方法估斜率：

```text
slope ≈ (phase_end - phase_start) / (t_end - t_start)
```

或者用累加器做一阶拟合。

现在先理解概念：

```text
频偏 -> phase 线性斜坡
斜率 -> 频偏估计
减掉斜坡 -> 保留真正的调制相位
```

## 6. 小变形检查题

### 题 1

如果相位斜率是：

```text
slope = 2*pi*100
```

那么频偏 `Delta_f` 是多少？

### 题 2

为什么 CW 去线性趋势后 `phase_span` 会变小？

### 题 3

为什么 FM 去线性趋势后不会把真正的 FM 正弦相位摆动也完全减掉？
