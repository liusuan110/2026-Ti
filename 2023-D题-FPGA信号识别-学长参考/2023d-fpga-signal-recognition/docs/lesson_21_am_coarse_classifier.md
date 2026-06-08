# 2023D 第二十一课：AM 粗分类器

上一课我们有了幅度特征：

```text
mag_sq = I^2 + Q^2
```

这一课开始接近题目的“自动识别”：

```text
先判断：这是不是 AM？
```

## 1. 为什么用 mag_sq 判断 AM

AM 改变的是幅度。

DDC 后：

```text
sqrt(I^2 + Q^2) ≈ 幅度
```

所以：

```text
I^2 + Q^2 ≈ 幅度^2
```

如果是 AM：

```text
mag_sq 会明显起伏
```

如果是 FM 或 CW：

```text
理想情况下 mag_sq 基本稳定
```

所以 AM 粗分类可以先看：

```text
mag_sq 在一段时间内变化大不大
```

## 2. 直接看最大最小

在一个观察窗口里计算：

```text
mag_min = min(mag_sq)
mag_max = max(mag_sq)
range = mag_max - mag_min
```

如果：

```text
range 很大
```

说明幅度起伏明显，更像 AM。

但只看 `range` 不够，因为信号整体变大时，range 也会一起变大。

所以要做相对判断。

## 3. 不用除法的相对判断

可以定义：

```text
score = range / (mag_max + mag_min)
```

但是 FPGA 里除法比较贵。

所以不用真的算除法，改成比较：

```text
range * 100 >= (mag_max + mag_min) * THRESHOLD_PERCENT
```

比如阈值取：

```text
THRESHOLD_PERCENT = 25
```

就等价于：

```text
range / (mag_max + mag_min) >= 0.25
```

这就是硬件里很常见的做法：

```text
把除法判断改成乘法比较。
```

## 4. 为什么阈值可以先取 25%

题目里 AM 调制度范围是：

```text
0.3 < ma < 1
```

AM 的幅度平方最小最大大约是：

```text
min ≈ (1-ma)^2
max ≈ (1+ma)^2
```

所以：

```text
(max-min)/(max+min) = 2ma/(1+ma^2)
```

当 `ma = 0.3` 时：

```text
2ma/(1+ma^2) ≈ 0.55
```

这比 0.25 大很多。

所以教学版先用 25% 作为粗分类阈值，是比较保守的。

真实硬件有噪声、滤波残留、增益变化，阈值以后还要实测调整。

## 5. Verilog 模块

文件：

```text
rtl/am_coarse_classifier.v
```

它做的事情：

```text
在 BLOCK_LEN 个有效样本里统计 mag_sq 的最大值和最小值
计算 range = max - min
计算 sum = max + min
比较 range*100 和 sum*THRESHOLD_PERCENT
输出 is_am
```

核心判断：

```verilog
is_am <= (range_scaled >= threshold_scaled);
```

其中：

```text
range_scaled     = range * 100
threshold_scaled = (max + min) * THRESHOLD_PERCENT
```

## 6. 为什么观察窗口要足够长

题目里 AM 调制频率可能是：

```text
1 kHz, 2 kHz, 3 kHz, 4 kHz, 5 kHz
```

最低 1 kHz 的周期是：

```text
1 ms
```

如果采样率是 20 MHz：

```text
1 ms = 20000 个采样点
```

所以观察窗口不能太短。

这一课先取：

```text
BLOCK_LEN = 32768
```

大约是：

```text
32768 / 20MHz ≈ 1.64 ms
```

足够覆盖 1 kHz AM 的一个完整周期。

## 7. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_21_am_coarse_classifier.py
```

输出：

```text
outputs/lesson_21/am_coarse_classifier_results.csv
outputs/lesson_21/am_coarse_classifier.png
```

脚本会测试：

```text
AM weak   ma=0.3, F=1kHz
AM strong ma=0.7, F=3kHz
FM
CW
```

## 8. 这一课要记住

```text
AM 粗分类不是先精确估 ma。
而是先问：幅度特征有没有明显起伏？
```

硬件判断可以先写成：

```text
统计一段时间内 mag_sq 的 max/min
看相对变化是否超过阈值
```

这一步不完美，但它简单、可解释、容易调试。

## 9. 小变形检查题

### 题 1

为什么不能只看 `max-min`，而要看它相对 `max+min` 的比例？

### 题 2

为什么 `BLOCK_LEN` 不能太短？

### 题 3

为什么硬件里常把：

```text
range / sum > threshold
```

改写成：

```text
range * 常数 > sum * 阈值常数
```
