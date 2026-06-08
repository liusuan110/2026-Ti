# 2023D 第二十三课：AM 调制频率 F 的估计

现在 AM 分支已经有了：

```text
AM 粗分类
ma 粗估计
```

这一课估计题目还要求的：

```text
调制频率 F
```

题目给出的候选只有：

```text
1 kHz, 2 kHz, 3 kHz, 4 kHz, 5 kHz
```

所以我们不用做连续频率搜索，只需要：

```text
给 5 个候选频率分别打分
分数最大的就是估计结果
```

## 1. 用哪个信号估 F

AM 的信息在幅度里。

前面我们已经有：

```text
mag_sq = I^2 + Q^2
```

对 AM 来说：

```text
mag_sq 会按调制频率起伏
```

所以估计 AM 的 `F`，就从 `mag_sq` 里估。

## 2. 先去掉直流

`mag_sq` 里面有很大的平均值。

相关法更关心“起伏部分”，所以软件里会先做：

```text
feature_ac = mag_sq - mean(mag_sq)
```

硬件里如果观察窗口刚好覆盖候选频率的整数周期，参考 sin/cos 的均值接近 0，那么直流项和参考信号相乘累加后会自然抵消很多。

但概念上要记住：

```text
估 F 看的是 mag_sq 的交流起伏，不是它的直流大小。
```

## 3. 相关法打分

对每个候选频率 `f`，计算：

```text
I_score = sum(feature_ac * cos(2*pi*f*t))
Q_score = sum(feature_ac * sin(2*pi*f*t))
score = I_score^2 + Q_score^2
```

如果 `feature_ac` 里真的有这个频率，乘上对应参考后会同向累加，分数大。

如果频率不匹配，正负会互相抵消，分数小。

最后：

```text
estimated_F = score 最大的候选频率
```

## 4. 为什么要同时用 sin 和 cos

因为 AM 包络的起伏不一定从正弦的 0 相位开始。

如果只和 cos 比，初始相位不对时分数可能变小。

同时算：

```text
cos 分量
sin 分量
```

再用：

```text
I_score^2 + Q_score^2
```

就对初始相位不敏感。

这和前面 I/Q 的道理是一致的：

```text
两个正交分量合起来表示完整的幅度。
```

## 5. 观察窗口为什么取 40000

这一课用：

```text
BLOCK_LEN = 40000
FS = 20 MHz
```

所以窗口时长是：

```text
40000 / 20MHz = 2 ms
```

这对候选频率很方便：

```text
1 kHz -> 2 个周期
2 kHz -> 4 个周期
3 kHz -> 6 个周期
4 kHz -> 8 个周期
5 kHz -> 10 个周期
```

也就是说：

```text
所有候选频率都刚好是整数周期。
```

这样相关分数更干净，直流项也更容易抵消。

## 6. 硬件对应模块

文件：

```text
rtl/tone_score_accumulator.v
```

它是一个通用的“单频打分器”：

```text
输入 feature
输入 ref_cos/ref_sin
累加 feature*ref_cos 和 feature*ref_sin
输出 score_power = I_score^2 + Q_score^2
```

真正估 5 个候选频率时，可以实例化 5 个：

```text
1 kHz scorer
2 kHz scorer
3 kHz scorer
4 kHz scorer
5 kHz scorer
```

然后比较 5 个 `score_power`，取最大。

参考 `ref_cos/ref_sin` 可以用低频 DDS 产生，方法和 2 MHz DDS 是一样的，只是 `phase_step` 不同。

## 7. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_23_am_frequency_estimator.py
```

输出：

```text
outputs/lesson_23/am_frequency_scores.csv
outputs/lesson_23/am_frequency_summary.csv
outputs/lesson_23/am_frequency_scores.png
outputs/lesson_23/am_frequency_feature_example.png
```

脚本会测试：

```text
ma = 0.3, 0.6, 0.9
F = 1,2,3,4,5 kHz
```

共 15 个 AM 场景。

## 8. 这一课要记住

```text
AM 的 F 来自幅度特征的起伏频率。
```

因为候选频率很少，所以第一版硬件可以：

```text
5 个候选频率分别相关打分
谁最大选谁
```

这比做完整频谱简单很多，也更贴合题目。

## 9. 小变形检查题

### 题 1

为什么 AM 的 `F` 要从 `mag_sq` 里估，而不是从 `phase` 里估？

### 题 2

为什么相关法要同时算 `cos` 分数和 `sin` 分数？

### 题 3

为什么这一课选 `BLOCK_LEN = 40000` 会让 1k 到 5k 的候选频率更好处理？
