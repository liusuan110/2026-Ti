# 2023D 第二十课：AM 幅度特征的硬件近似

上一课得到结论：

```text
I/Q 是坐标
半径是幅度
角度是相位
```

理论上的幅度是：

```text
magnitude = sqrt(I^2 + Q^2)
```

但是 FPGA 里直接开方不是最便宜的操作。

这一课讲：

```text
硬件里可以先用哪些幅度特征近似它。
```

## 1. 最准确但比较贵：I^2 + Q^2

如果不想开方，可以先算：

```text
mag_sq = I^2 + Q^2
```

它是半径的平方。

优点：

```text
不受 I/Q 相位角影响
非常适合判断“幅度有没有变化”
```

缺点：

```text
需要两个乘法器
结果位宽很大
如果要恢复线性幅度，还需要开方或近似开方
```

所以 `mag_sq` 很适合做 AM/CW/FM 的粗判断：

```text
AM -> mag_sq 明显起伏
FM/CW -> mag_sq 相对稳定
```

但如果要非常准确地估计 `ma`，后面还要处理平方关系。

## 2. 最便宜但误差大：|I| + |Q|

一个很便宜的特征是：

```text
abs_sum = |I| + |Q|
```

优点：

```text
只需要取绝对值和加法
很便宜
```

缺点：

```text
它会随相位角变化
```

例如同样半径：

```text
I=1, Q=0       -> |I|+|Q| = 1
I=0.707,Q=0.707 -> |I|+|Q| = 1.414
```

这意味着它会把相位变化误认为幅度变化。

## 3. 折中近似：max + min/2

更好一点的便宜近似是：

```text
max_abs = max(|I|, |Q|)
min_abs = min(|I|, |Q|)
mag_approx = max_abs + min_abs/2
```

硬件里：

```text
min_abs/2 = min_abs >> 1
```

优点：

```text
不用乘法
不用开方
比 |I|+|Q| 更接近真实半径
```

缺点：

```text
仍然有相位相关误差
不是精确幅度
```

所以它适合：

```text
教学版 AM 特征
粗分类
先把系统跑通
```

不适合直接当最终高精度 `ma` 估计。

## 4. Verilog 模块

文件：

```text
rtl/iq_magnitude_approx.v
```

它输出三类特征：

```text
abs_i
abs_q
mag_approx = max(|I|,|Q|) + min(|I|,|Q|)/2
mag_sq = I^2 + Q^2
```

核心思路：

```text
先求 |I| 和 |Q|
再比较大小
再得到近似半径和半径平方
```

## 5. 为什么 abs 要多一位

如果输入是 21 bit signed，它的范围是：

```text
-2^20 到 2^20-1
```

最小负数的绝对值是：

```text
2^20
```

这需要 22 bit 才能表示。

所以模块里：

```text
ABS_BITS = DATA_BITS + 1
```

这是为了避免取绝对值时溢出。

## 6. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_20_magnitude_feature.py
```

输出：

```text
outputs/lesson_20/magnitude_angle_sweep.csv
outputs/lesson_20/magnitude_am_tracking.csv
outputs/lesson_20/magnitude_angle_error.png
outputs/lesson_20/magnitude_am_tracking.png
```

脚本会对比：

```text
精确 sqrt(I^2+Q^2)
|I|+|Q|
max+min/2
I^2+Q^2
```

## 7. 这节课怎么用于 2023D

现在我们可以做一个初版 AM 判断：

```text
如果幅度特征随时间明显起伏 -> AM
如果幅度特征基本稳定 -> 不是 AM
```

教学版可以先用：

```text
mag_approx
```

更稳一点可以用：

```text
mag_sq
```

但后面如果要严格估计 `ma`，我们会再讨论：

```text
平方幅度怎么换回线性幅度
或者用 CORDIC 求更准确的 magnitude
```

## 8. 这一课要记住

```text
sqrt(I^2+Q^2) 是理论幅度。
FPGA 里可以先用便宜特征近似它。
```

简单分类时：

```text
不一定一开始就要精确开方。
```

但要知道代价：

```text
近似越便宜，误差和适用范围越要小心。
```

## 9. 小变形检查题

### 题 1

为什么 `I^2 + Q^2` 不需要开方，也可以用来判断 AM 幅度有没有变化？

### 题 2

为什么 `|I| + |Q|` 会受到相位角影响？

### 题 3

为什么 21 bit signed 数据取绝对值时，最好用 22 bit 保存结果？
