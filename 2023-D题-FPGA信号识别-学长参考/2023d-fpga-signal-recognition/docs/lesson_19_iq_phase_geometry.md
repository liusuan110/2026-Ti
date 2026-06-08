# 2023D 第十九课：I/Q 为什么像坐标

上一课测试的是同相输入：

```text
x(t) = A cos(omega t)
```

低通后：

```text
I 比较大
Q 接近 0
```

这一课把输入相位加进去：

```text
x(t) = A cos(omega t + phi)
```

看看 I/Q 会变成什么。

## 1. I 路结果

I 路乘的是：

```text
cos(omega t)
```

所以：

```text
I_raw = A cos(omega t + phi) cos(omega t)
```

积化和差：

```text
cos(a)cos(b) = 1/2 cos(a-b) + 1/2 cos(a+b)
```

令：

```text
a = omega t + phi
b = omega t
```

得到：

```text
I_raw = A/2 cos(phi) + A/2 cos(2 omega t + phi)
```

低通滤掉高频项：

```text
I ≈ A/2 cos(phi)
```

## 2. Q 路结果

Q 路乘的是：

```text
-sin(omega t)
```

所以：

```text
Q_raw = A cos(omega t + phi) [-sin(omega t)]
```

低通后会留下：

```text
Q ≈ A/2 sin(phi)
```

所以总结：

```text
I ≈ A/2 cos(phi)
Q ≈ A/2 sin(phi)
```

这就是 I/Q 像坐标的原因。

## 3. I/Q 点是什么意思

可以把一个信号的基带状态看成平面上的一个点：

```text
(I, Q)
```

如果：

```text
phi = 0 度
```

那么：

```text
I 大，Q 接近 0
```

如果：

```text
phi = 90 度
```

那么：

```text
I 接近 0，Q 大
```

如果：

```text
phi = 180 度
```

那么：

```text
I 为负，Q 接近 0
```

这和圆上的坐标完全一致。

## 4. 幅度和相位从哪里来

因为：

```text
I = A/2 cos(phi)
Q = A/2 sin(phi)
```

所以：

```text
sqrt(I^2 + Q^2) = A/2
```

这就是幅度特征。

同时：

```text
atan2(Q, I) = phi
```

这就是相位特征。

这两句是后面识别 AM/FM 的关键：

```text
AM 主要看 sqrt(I^2 + Q^2)
FM 主要看 atan2(Q, I)
```

## 5. 为什么这能解释 AM

AM 会让幅度变：

```text
A(t) = A0 [1 + ma sin(2*pi*F*t)]
```

于是：

```text
sqrt(I^2 + Q^2) ≈ A(t)/2
```

所以 AM 的包络会出现在 I/Q 的半径里。

## 6. 为什么这能解释 FM

FM 会让相位变：

```text
phi(t) = mf sin(2*pi*F*t)
```

于是：

```text
atan2(Q, I) ≈ phi(t)
```

所以 FM 的调制会出现在 I/Q 的角度里。

## 7. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_19_iq_phase_geometry.py
```

输出：

```text
outputs/lesson_19/iq_phase_summary.csv
outputs/lesson_19/iq_phase_geometry.png
```

脚本会测试这些输入相位：

```text
0 度
30 度
90 度
-60 度
150 度
```

然后画出 I/Q 点。

## 8. 这一课要记住

```text
I/Q 不是两条随便来的波形。
它们是同一个信号在“同相轴”和“正交轴”上的投影。
```

更短一点：

```text
I/Q 是信号的坐标。
```

所以：

```text
半径 -> 幅度
角度 -> 相位
```

## 9. 小变形检查题

### 题 1

如果输入相位 `phi = 90 度`，低通后的 I 应该接近 0，还是 Q 应该接近 0？

### 题 2

如果 `I` 和 `Q` 同时按相同比例变大，`atan2(Q, I)` 会不会改变？

### 题 3

为什么 AM 更适合看 I/Q 的半径，而 FM 更适合看 I/Q 的角度？
