# 2023D 第二十五课：FM/CW 分类器

现在整体分类可以分两步：

```text
第一步：用 mag_sq 判断是不是 AM
第二步：如果不是 AM，再用 phase 判断是 FM 还是 CW
```

这一课讲第二步。

## 1. FM 和 CW 在 phase 上的区别

CW 理想模型：

```text
x(t) = A cos(2*pi*fc*t + phi0)
```

DDC 后：

```text
phase(t) ≈ 常数
```

如果有载波频偏，会多一条斜坡：

```text
phase(t) ≈ 2*pi*Delta_f*t + 常数
```

去掉斜坡后：

```text
phase_detrended ≈ 常数
```

FM 模型：

```text
x(t) = A cos(2*pi*fc*t + mf*sin(2*pi*F*t))
```

DDC 并去斜坡后：

```text
phase_detrended ≈ mf*sin(2*pi*F*t)
```

所以：

```text
CW：phase_detrended 几乎不动
FM：phase_detrended 明显摆动
```

## 2. 最简单的分类特征

可以在一个窗口内统计：

```text
phase_min
phase_max
phase_range = phase_max - phase_min
```

如果：

```text
phase_range 很小 -> CW
phase_range 很大 -> FM
```

因为题目 FM 调制指数范围是：

```text
1 <= mf <= 5
```

理想 FM 的 phase 摆幅至少约：

```text
正负 1 rad
```

所以 FM 和 CW 在 phase 摆动幅度上很好分。

## 3. 更稳一点：用候选频率相关幅度

只看最大最小值容易受噪声尖峰影响。

更稳的方法是：

```text
对 phase_detrended 做 1k~5k 候选频率打分
找最大分数对应的相位幅度 phase_amp
```

如果：

```text
phase_amp >= threshold -> FM
phase_amp < threshold  -> CW
```

这一课脚本取：

```text
threshold = 0.45 rad
```

因为题目里最小 FM 是 `mf=1`，留出一些余量后，0.45 是一个教学版阈值。

## 4. Verilog 模块

文件：

```text
rtl/fm_cw_classifier.v
```

这个模块做的是简单版：

```text
统计 phase_detrended 的 max/min
看 phase_range 是否超过阈值
```

它假设输入已经是：

```text
去掉频偏斜坡后的 phase
```

注意：它不负责算 `atan2`，也不负责 detrend。

这些以后由：

```text
CORDIC phase
phase detrend
```

前级模块完成。

## 5. fixed-point 阈值

RTL 默认假设 phase 用：

```text
Q4.20 radians
```

也就是：

```text
1 rad ≈ 2^20
```

如果阈值取：

```text
phase_range ≈ 0.9 rad
```

那么整数阈值约：

```text
0.9 * 2^20 ≈ 943718
```

这就是模块里的默认：

```verilog
PHASE_RANGE_THRESHOLD = 25'd943718
```

以后如果 phase 的定点格式变了，这个阈值也要跟着改。

## 6. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_25_fm_cw_classifier.py
```

输出：

```text
outputs/lesson_25/fm_cw_classifier_summary.csv
outputs/lesson_25/fm_cw_classifier_scores.csv
outputs/lesson_25/fm_cw_classifier.png
```

脚本测试：

```text
FM mf=1 F=1k
FM mf=3 F=5k offset=200Hz
FM mf=5 F=2k
CW
CW offset=200Hz
CW phase=90deg
```

## 7. 这一课要记住

```text
AM / FM / CW 可以分层判断：
先用幅度起伏找 AM；
再用相位起伏区分 FM 和 CW。
```

对 non-AM：

```text
phase_detrended 摆动明显 -> FM
phase_detrended 近似平 -> CW
```

## 8. 小变形检查题

### 题 1

为什么 CW 在去掉频偏斜坡后，`phase_detrended` 应该接近常数？

### 题 2

为什么 FM 的 `phase_detrended` 会明显摆动？

### 题 3

为什么用相关幅度判断 FM，比只看 `phase_max - phase_min` 更抗噪声？
