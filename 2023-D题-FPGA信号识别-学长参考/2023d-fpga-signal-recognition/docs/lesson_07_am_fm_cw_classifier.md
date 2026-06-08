# 2023D 第七课：AM / FM / CW 自动识别

前面我们分别做了：

```text
AM：包络、m_a、F
FM：相位、m_f、F、Delta_f_max
CW：幅度和相位基本不变
```

这一课把它们合起来，做基本要求（3）：

```text
未知 u_M 调制方式条件下，装置可自主识别并显示 u_M 的调制方式。
```

## 1. 统一用 DDC 后的 I/Q 特征

先对输入信号做 DDC：

```text
x[n] -> I[n], Q[n]
```

然后看两个特征：

```text
magnitude[n] = sqrt(I[n]^2 + Q[n]^2)
phase[n] = atan2(Q[n], I[n])
```

## 2. AM 的特征

AM 的信息在幅度里：

```text
magnitude 明显变化
phase 基本不大幅变化
```

所以可以计算：

```text
mag_depth = (mag_max - mag_min) / (mag_max + mag_min)
```

如果 `mag_depth` 明显大，就判为 AM。

## 3. FM 的特征

FM 的信息在相位里：

```text
magnitude 基本不变
phase 明显变化
```

所以可以计算：

```text
phase_span = (phase_max - phase_min) / 2
```

如果 `mag_depth` 不大，但 `phase_span` 明显大，就判为 FM。

## 4. CW 的特征

CW 没有幅度调制，也没有频率调制：

```text
magnitude 基本恒定
phase 基本恒定
```

所以：

```text
mag_depth 小
phase_span 小
```

如果前两类都不满足，就判为 CW。

## 5. 当前简单判决规则

Python 脚本里用了一个教学用规则：

```text
if mag_depth > 0.18:
    AM
else if phase_span > 0.45:
    FM
else:
    CW
```

这不是最终比赛阈值，只是为了说明识别逻辑。

真正上硬件后，阈值需要根据：

```text
ADC 噪声
前端放大器增益
信号源幅度
是否削顶
I/Q 滤波窗口
```

做实测调整。

## 6. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_07_am_fm_cw_classifier.py
```

输出：

```text
outputs/lesson_07/am_fm_cw_classifier.png
```

## 7. 小变形检查题

### 题 1

如果一个信号的：

```text
mag_depth 很大
phase_span 很小
```

更像 AM、FM 还是 CW？

### 题 2

如果一个信号的：

```text
mag_depth 很小
phase_span 很大
```

更像 AM、FM 还是 CW？

### 题 3

为什么当前阈值 `0.18` 和 `0.45` 不能直接当作最终硬件阈值？
