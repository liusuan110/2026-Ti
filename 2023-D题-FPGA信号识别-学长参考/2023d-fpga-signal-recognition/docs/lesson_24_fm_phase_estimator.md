# 2023D 第二十四课：FM 分支的相位估计

AM 分支已经能估：

```text
F
ma
```

现在开始 FM 分支。

FM 的题目目标是估：

```text
F
mf
Delta_fmax
```

其中：

```text
Delta_fmax = mf * F
```

## 1. FM 的信息在哪里

FM 模型是：

```text
x(t) = A cos(2*pi*fc*t + mf*sin(2*pi*F*t))
```

DDC 后，I/Q 的角度近似是：

```text
phase(t) = atan2(Q, I)
         ≈ mf*sin(2*pi*F*t)
```

所以：

```text
FM 的 F 在 phase 的起伏频率里
FM 的 mf 在 phase 的起伏幅度里
```

这和 AM 正好对应：

```text
AM 看半径
FM 看角度
```

## 2. 先去频偏斜坡

真实系统里载波可能不是精确 2 MHz。

如果真实载波是：

```text
fc + Delta_f
```

而 DDC 用的是：

```text
fc
```

那么 phase 里会多出：

```text
2*pi*Delta_f*t
```

这是一条斜坡。

所以 FM 分支先做：

```text
phase_detrended = phase - phase_line
```

再用剩下的周期性摆动估 `F` 和 `mf`。

这一课脚本用端点法估斜坡：

```text
slope ≈ (phase[stop] - phase[start]) / (t[stop] - t[start])
```

因为本课窗口取 2 ms，对 1k~5k 都是整数个调制周期，起点和终点的 FM 正弦相位理论上相同。这样估出来的斜坡更接近载波频偏，不容易把真正的 FM 摆动也当作斜坡减掉。

## 3. 估 F：还是候选频率打分

题目里调制频率候选还是：

```text
1 kHz, 2 kHz, 3 kHz, 4 kHz, 5 kHz
```

所以和 AM 估 F 类似：

```text
对 phase_detrended 分别和 1k~5k 的 sin/cos 相关
谁的 score 最大，就估成谁
```

公式：

```text
I_score = sum(phase_detrended * cos(2*pi*f*t))
Q_score = sum(phase_detrended * sin(2*pi*f*t))
score = I_score^2 + Q_score^2
```

## 4. 估 mf：相位正弦的幅度

理想情况下：

```text
phase_detrended(t) = mf*sin(2*pi*F*t)
```

所以它的正弦幅度就是：

```text
mf
```

教学版可以用两种方式：

```text
mf ≈ (phase_max - phase_min)/2
```

或者用相关幅度：

```text
mf ≈ 2*sqrt(I_score^2 + Q_score^2)/N
```

相关幅度比 max/min 更抗噪声。

这一课脚本主要用相关幅度。

## 5. 估 Delta_fmax

FM 调制指数和最大频偏的关系是：

```text
mf = Delta_fmax / F
```

所以：

```text
Delta_fmax = mf * F
```

题目要求 `Delta_fmax` 误差不大于：

```text
300 Hz
```

所以 `F` 和 `mf` 都要尽量稳。

## 6. FPGA 里缺的关键模块

这节 Python 用了：

```python
phase = np.unwrap(np.arctan2(Q, I))
```

FPGA 里不会直接调用 `atan2`。

以后要用：

```text
CORDIC
```

从 I/Q 计算 phase。

所以当前 FPGA 路线是：

```text
DDC Core -> I/Q
I/Q -> CORDIC phase
phase -> 去斜坡
phase_detrended -> 候选频率打分
phase_detrended -> mf 估计
```

这节课先把算法链路确认好，下一步再讲 CORDIC 或 phase 统计模块。

## 7. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_24_fm_phase_estimator.py
```

输出：

```text
outputs/lesson_24/fm_phase_estimator_summary.csv
outputs/lesson_24/fm_phase_estimator_scores.csv
outputs/lesson_24/fm_phase_estimator.png
```

脚本测试：

```text
FM mf=1, F=1k
FM mf=3, F=5k, 载波偏 +200Hz
FM mf=5, F=2k
CW, 载波偏 +200Hz
```

## 8. 这一课要记住

```text
FM 的本质是相位随时间摆动。
```

所以 FM 分支的主特征是：

```text
phase_detrended = 去掉频偏斜坡后的 atan2(Q,I)
```

然后：

```text
F  -> phase_detrended 的起伏频率
mf -> phase_detrended 的起伏幅度
Delta_fmax -> mf * F
```

## 9. 小变形检查题

### 题 1

为什么 FM 的 `F` 要从 `phase` 里估，而不是从 `mag_sq` 里估？

### 题 2

为什么 FM 分支要先做 `phase detrend`？

### 题 3

如果估得：

```text
F = 5 kHz
mf = 3
```

那么：

```text
Delta_fmax = ?
```
