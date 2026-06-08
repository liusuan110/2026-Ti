# 2023D 第十一课：把频偏补偿接进完整识别流程

前面几课已经有了这些积木：

```text
DDC/IQ
幅度 magnitude
相位 phase
AM/FM/CW 分类
AM 参数估计
FM 参数估计
频偏导致 phase 斜坡
phase detrend 去掉斜坡
```

这一课把它们接成一个更稳的流程。

## 1. 第九课哪里失败了

第九课里，如果输入载波是：

```text
2 MHz + 200 Hz
```

但本地 DDC 仍然按：

```text
2 MHz
```

去下变频，那么基带相位会多出：

```text
2*pi*200*t
```

这是一条斜坡。

后果是：

```text
CW 会被误判成 FM
FM 的 m_f 会被估得偏大
```

## 2. 新流程

新的分析流程是：

```text
输入 ADC 波形
    ↓
减掉直流偏置
    ↓
DDC 得到 I/Q
    ↓
计算 magnitude 和 phase
    ↓
对 phase 拟合直线
    ↓
phase_detrended = phase - phase_line
    ↓
用 magnitude 判断 AM
用 phase_detrended 判断 FM/CW
```

重点是最后两句：

```text
AM 看 magnitude
FM/CW 看去斜坡后的 phase
```

## 3. 为什么 AM 不太怕频偏

AM 的主要信息在幅度包络里：

```text
magnitude(t) ≈ A(t)
```

载波频偏主要影响相位：

```text
phase(t) 多一条斜坡
```

所以 AM 识别通常先看 `magnitude`，不急着看 phase。

## 4. 为什么 FM 需要 phase detrend

FM 的相位模型是：

```text
phase(t) ≈ 2*pi*Delta_f*t + m_f*sin(2*pi*F*t) + const
```

其中：

```text
2*pi*Delta_f*t        频偏造成的斜坡
m_f*sin(2*pi*F*t)     真正的 FM 调制相位
```

如果直接用最大值最小值算 `m_f`，斜坡会把范围拉大。

所以先减掉斜坡，再估计：

```text
m_f
F
Delta_f_max = m_f * F
```

## 5. 为什么这课不用 max-min 估 m_f

第八课为了直观，用：

```text
m_f ≈ (phase_max - phase_min) / 2
```

这很好懂，但容易受边缘误差、噪声、滤波暂态影响。

第十一课改成用候选频率的正弦/余弦相关来估相位摆幅：

```text
mf_est ≈ phase_detrended 在目标频率上的正弦幅度
```

这更接近后面 FPGA 里的做法：

```text
不是只盯最大最小两个点
而是让一整段采样都参与投票
```

## 6. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_11_frequency_compensated_pipeline.py
```

输出：

```text
outputs/lesson_11/frequency_compensated_pipeline_results.csv
outputs/lesson_11/frequency_compensated_pipeline_summary.png
```

## 7. 这一课要记住

```text
频偏补偿不是为了让信号“更漂亮”。
它是为了不让频偏伪装成 FM。
```

更工程一点说：

```text
先把已知干扰项拿掉，
再测真正关心的调制项。
```

## 8. 小变形检查题

### 题 1

如果一个 CW 信号下变频后 `phase(t)` 是斜线，这更像是：

```text
A. AM 调制度很大
B. 载波频率有偏差
C. 输入幅度太大
```

### 题 2

为什么新版流程里 AM 先看 `magnitude`，而不是先看 `phase`？

### 题 3

为什么用一整段相关来估 `m_f`，通常比只用最大值和最小值更稳？
