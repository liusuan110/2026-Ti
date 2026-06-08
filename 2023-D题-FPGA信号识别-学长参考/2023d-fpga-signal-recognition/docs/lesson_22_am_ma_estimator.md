# 2023D 第二十二课：从 mag_sq 估计 AM 调制度 ma

上一课我们已经能粗判断：

```text
这是 AM，还是 non-AM
```

这一课进一步估计题目要求的：

```text
AM 调制度 ma
```

## 1. 先回到 AM 模型

AM 包络是：

```text
E(t) = 1 + ma * sin(2*pi*F*t)
```

所以包络最大最小值是：

```text
Emax = 1 + ma
Emin = 1 - ma
```

如果我们拿到的是线性幅度，那么：

```text
ma = (Emax - Emin) / (Emax + Emin)
```

这个公式前面已经推过。

## 2. 现在的问题：我们手里是 mag_sq

第 21 课用的是：

```text
mag_sq = I^2 + Q^2
```

它不是线性幅度，而是幅度平方。

如果线性幅度最大最小是：

```text
Emax
Emin
```

那么 `mag_sq` 的最大最小近似是：

```text
Mmax ∝ Emax^2
Mmin ∝ Emin^2
```

所以不能直接写：

```text
ma = (Mmax - Mmin) / (Mmax + Mmin)
```

这样会明显偏大。

## 3. 精确一点的写法：先开方

如果允许开方：

```text
sqrt(Mmax) ∝ Emax
sqrt(Mmin) ∝ Emin
```

所以：

```text
ma = (sqrt(Mmax) - sqrt(Mmin)) / (sqrt(Mmax) + sqrt(Mmin))
```

这是从 `mag_sq` 恢复线性幅度的直接方法。

缺点是：

```text
硬件里开方和除法都不便宜。
```

## 4. 不开方也能推公式

第 21 课定义了：

```text
score = (Mmax - Mmin) / (Mmax + Mmin)
```

对 AM 来说：

```text
Mmax ∝ (1 + ma)^2
Mmin ∝ (1 - ma)^2
```

代入可得：

```text
score = 2*ma / (1 + ma^2)
```

反过来解：

```text
ma = (1 - sqrt(1 - score^2)) / score
```

这说明：

```text
score 和 ma 是一一对应的。
```

所以即使不直接开方 `Mmax/Mmin`，也可以从 `score` 推回 `ma`。

## 5. 硬件教学版：用阈值查表

题目要求：

```text
ma 误差不大于 0.1
```

所以第一版 FPGA 不一定要输出很多小数位。

可以先估成：

```text
0.3, 0.4, 0.5, ..., 0.9
```

怎么做？

根据：

```text
score = 2*ma/(1+ma^2)
```

可以提前算好分界线。

比如 0.3 和 0.4 的中间按 `ma=0.35` 算：

```text
score ≈ 0.623
```

0.4 和 0.5 的中间按 `ma=0.45` 算：

```text
score ≈ 0.748
```

于是硬件里比较：

```text
score < 0.623 -> ma ≈ 0.3
score < 0.748 -> ma ≈ 0.4
...
```

但我们仍然不做除法。

把：

```text
score = range / sum
```

改成：

```text
range * 1000 < sum * threshold_permille
```

这就是 `rtl/am_ma_estimator_lut.v` 的思路。

## 6. Verilog 模块

文件：

```text
rtl/am_ma_estimator_lut.v
```

输入：

```text
block_min
block_max
```

输出：

```text
ma_permille
```

其中：

```text
300 表示 ma = 0.300
400 表示 ma = 0.400
...
900 表示 ma = 0.900
```

它不做除法，也不做开方，只做：

```text
range * 1000
sum * 阈值常数
比较大小
```

## 7. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_22_am_ma_estimator.py
```

输出：

```text
outputs/lesson_22/am_ma_estimator_results.csv
outputs/lesson_22/am_ma_estimator.png
```

脚本会比较四种估计：

```text
naive score       直接把 score 当 ma，错误示范
sqrt max/min      先开方再估，比较准确
score formula     从 score 公式反推 ma，比较准确
LUT nearest 0.1   硬件友好，输出 0.3~0.9 档位
```

## 8. 这一课要记住

```text
mag_sq 是幅度平方。
用它估 ma 时，不能把平方幅度当线性幅度。
```

第一版硬件可以用：

```text
max/min -> score -> 查表得到 ma 档位
```

先满足题目误差，再逐步优化。

## 9. 小变形检查题

### 题 1

为什么不能直接把：

```text
(Mmax - Mmin)/(Mmax + Mmin)
```

当作 `ma`？

### 题 2

如果 `ma` 只需要误差不超过 0.1，为什么查表输出 0.3、0.4、...、0.9 是合理的第一版？

### 题 3

为什么硬件版查表仍然不用真的计算：

```text
score = range / sum
```

而是用乘法比较？
