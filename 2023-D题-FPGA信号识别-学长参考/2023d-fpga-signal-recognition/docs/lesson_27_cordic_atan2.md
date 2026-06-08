# 2023D 第二十七课：CORDIC / atan2 硬件思路

前面的问题是：

```text
detrend 硬件是不是还要买模块？
```

不用买。

这里的“硬件”指：

```text
写在 FPGA 里的数字逻辑。
```

FM/CW 分支里真正比较难的前置模块是：

```text
phase = atan2(Q, I)
```

这一课讲它在 FPGA 里常用的实现方法：

```text
CORDIC
```

## 1. atan2 是什么

前面学过：

```text
I = R cos(phi)
Q = R sin(phi)
```

所以：

```text
phi = atan2(Q, I)
```

普通 `atan(Q/I)` 有问题，因为它容易丢象限。

例如：

```text
I < 0, Q > 0
```

这是第二象限。`atan2(Q,I)` 能知道它在第二象限，普通 `atan(Q/I)` 容易判断错。

## 2. FPGA 为什么不用直接 atan 函数

Python 里可以写：

```python
phase = np.atan2(Q, I)
```

FPGA 里没有这样的高级函数可以每拍直接调用。

如果硬写多项式，会遇到：

```text
除法
范围判断
多项式乘法
象限修正
流水线
```

所以 FPGA 里常用：

```text
CORDIC
```

## 3. CORDIC 的直觉

CORDIC 的想法很朴素：

```text
不断用一组固定的小角度旋转向量，
把 (I,Q) 旋到接近 I 轴。
```

每一步只旋转这些角度：

```text
atan(1)
atan(1/2)
atan(1/4)
atan(1/8)
...
atan(2^-k)
```

为什么选这些？

因为乘以：

```text
2^-k
```

在硬件里就是右移。

所以每一步只需要：

```text
加法
减法
右移
查一个 atan 小表
```

这很适合 FPGA。

## 4. Vectoring 模式

我们要从 `(I,Q)` 求角度。

这叫：

```text
CORDIC vectoring mode
```

它不断让 `Q` 接近 0。

每一步大概是：

```text
如果 Q > 0，就往一个方向旋
如果 Q < 0，就往另一个方向旋
```

同时累计旋过的角度。

最后：

```text
累计角度 ≈ atan2(Q,I)
```

## 5. 角度表从哪里来

CORDIC 需要一张很小的表：

```text
atan(2^-0)
atan(2^-1)
atan(2^-2)
...
atan(2^-19)
```

脚本会生成：

```text
outputs/lesson_27/cordic_atan_table.csv
outputs/lesson_27/cordic_atan_q4_20.mem
```

这里 `Q4.20` 表示：

```text
角度用定点数表示
1 rad = 2^20
```

例如：

```text
pi ≈ 3.14159 rad
pi 的整数表示约为 3.14159 * 2^20
```

## 6. 为什么要预旋转

基本 CORDIC 一开始最擅长处理右半平面。

但 `atan2` 要覆盖四个象限。

所以如果：

```text
I < 0
```

可以先把向量转到右半平面，并给角度加上或减去 `pi`。

这叫：

```text
象限处理
```

没有这一步，第二、第三象限容易错。

## 7. RTL 骨架

文件：

```text
rtl/cordic_atan2_skeleton.v
```

它现在只是接口骨架，不是完整 CORDIC。

原因是完整 CORDIC 有两种写法：

```text
20 级流水线：每拍吞一个样本，资源较多，速度快
1 级迭代复用：20 拍算一个样本，资源少，速度慢
```

我们还没决定采用哪种结构，所以先固定接口：

```text
输入 I/Q
输出 phase
valid 跟随
```

后面再把真正的 CORDIC 级联逻辑填进去。

## 8. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_27_cordic_atan2.py
```

输出：

```text
outputs/lesson_27/cordic_atan_table.csv
outputs/lesson_27/cordic_atan2_sweep.csv
outputs/lesson_27/cordic_atan_q4_20.mem
outputs/lesson_27/cordic_atan2_error.png
```

脚本会用 Python 模拟 20 次 CORDIC 迭代，并和 `np.atan2` 对比。

## 9. 这一课要记住

```text
CORDIC 不是外接硬件。
CORDIC 是 FPGA 里用加法、移位和小表实现 atan2 的数字逻辑。
```

它的价值是：

```text
不用除法
不用复杂三角函数
适合流水线
能输出 phase 给 FM/CW 分支使用
```

## 10. 小变形检查题

### 题 1

为什么 `atan2(Q,I)` 比普通 `atan(Q/I)` 更适合从 I/Q 求相位？

### 题 2

CORDIC 为什么喜欢用 `atan(2^-k)` 这一组角度？

### 题 3

为什么完整 CORDIC 有“流水线版”和“迭代复用版”两种取舍？
