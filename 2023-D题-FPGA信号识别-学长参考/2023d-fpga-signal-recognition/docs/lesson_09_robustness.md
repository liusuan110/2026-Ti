# 2023D 第九课：鲁棒性测试

前面的 Python 流程用的是很干净的模拟信号。

真实硬件不会这么干净。可能出现：

```text
噪声
DC 偏置
输入幅度变化
ADC 量化误差
载波频率和本地 DDC 频率不完全一致
```

这一课先用 Python 故意加入这些问题，看看第八课总流程会不会坏。

## 1. 这节课故意加的干扰

脚本里定义了几个场景：

```text
clean：干净信号
low_amp：输入幅度变小
dc_offset：加入直流偏置
noise_5pct：加入 5% RMS 噪声
noise_12pct：加入 12% RMS 噪声
carrier_plus_200hz：输入载波比 2MHz 高 200Hz
carrier_plus_1000hz：输入载波比 2MHz 高 1000Hz
mixed_mild：幅度变小 + DC 偏置 + 噪声 + 载波频偏
```

这些都对应真实硬件里可能遇到的问题。

## 2. 为什么先做 DC 去除

真实 ADC 采样常常不是严格围绕 0。

所以 DDC 前先做：

```text
x = x - mean(x)
```

这叫 DC offset removal，直流偏置去除。

如果不去掉 DC 偏置，乘本地载波之后会出现额外干扰，影响 I/Q。

## 3. 幅度变化为什么通常还好

AM 的：

```text
m_a = (Emax - Emin) / (Emax + Emin)
```

FM 的：

```text
phase = atan2(Q, I)
```

都对整体幅度缩放不太敏感。

所以只要信号没有小到被量化噪声淹没，也没有大到削顶，幅度变化通常不会毁掉算法。

## 4. 噪声会影响什么

噪声会让：

```text
mag_depth 抖动
phase_span 抖动
候选频率分数变脏
```

当噪声较小时，长时间窗口和候选频率匹配会平均掉一部分噪声。

当噪声太大时，阈值和参数估计都会变差。

## 5. 载波频偏为什么危险

我们的 DDC 默认本地载波是：

```text
2 MHz
```

如果真实输入是：

```text
2 MHz + 1000 Hz
```

那么下变频后不会完全落到 0Hz，而会残留一个慢旋转。

这会让：

```text
I/Q 相位出现额外斜坡
FM 的 phase_span 变大
CW 可能被误判成 FM
```

所以真实硬件里，载波频率偏差是需要特别注意的。

可能的处理方式包括：

```text
1. 信号源载波设置尽量准确
2. DDC 本地频率可调
3. 先估计载波频偏，再补偿
4. 对 phase 去线性趋势，再估计 FM 相位摆动
```

先不用急着实现，知道这个风险就好。

## 6. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_09_robustness_test.py
```

输出：

```text
outputs/lesson_09/robustness_results.csv
outputs/lesson_09/robustness_summary.png
```

## 7. 小变形检查题

### 题 1

为什么整体幅度变大/变小，对 `m_a` 的影响不一定很大？

### 题 2

为什么载波频偏可能让 CW 看起来像 FM？

### 题 3

如果真实硬件里发现 CW 的 `phase_span` 很大，你会优先怀疑哪些问题？
