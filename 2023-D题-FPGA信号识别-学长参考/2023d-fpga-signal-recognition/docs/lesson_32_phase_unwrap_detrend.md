# 2023D 第三十二课：phase unwrap 和去频偏斜坡

第 31 课已经得到连续的：

```text
phase = atan2(Q, I)
```

但这个 phase 有两个问题：

```text
1. atan2 输出范围只有 -pi 到 +pi，跨界时会跳变
2. 载波频偏会让 phase 带一条线性斜坡
```

这一课解决这两个问题。

## 1. 为什么 phase 会跳变

`atan2` 的输出通常限制在：

```text
-pi 到 +pi
```

如果真实相位继续增大：

```text
3.13 -> 3.14 -> 3.15
```

输出可能变成：

```text
3.13 -> 3.14 -> -3.13
```

看起来突然跳了接近：

```text
-2*pi
```

这不是真实信号跳了，而是角度表示方式绕回去了。

## 2. unwrap 怎么修

核心规则：

```text
如果当前 phase - 上一个 phase < -pi，说明从 +pi 跳到了 -pi，加 2*pi
如果当前 phase - 上一个 phase > +pi，说明从 -pi 跳到了 +pi，减 2*pi
```

对应新模块：

```text
rtl/phase_unwrap.v
```

它输入 wrapped phase，输出连续 phase：

```text
phase_in -> phase_unwrapped_out
```

## 3. 为什么还要去斜坡

如果真实载波是：

```text
fc + Delta_f
```

但 DDC 用的是：

```text
fc
```

那么 phase 里会多出：

```text
2*pi*Delta_f*t
```

这是一条线性斜坡。

估出斜率：

```text
slope
```

就能估频偏：

```text
Delta_f = slope / (2*pi)
```

然后：

```text
phase_detrended = phase_unwrapped - fitted_line
```

## 4. 这一课做了什么

脚本：

```text
scripts/lesson_32_prepare_and_check_phase_unwrap_detrend.py
```

做两件事：

```text
1. 生成一个会多次跨过 ±pi 的合成 phase，验证 phase_unwrap RTL
2. 读取第 31 课输出，估计 200 Hz 载波频偏，并去掉相位斜坡
```

Vivado/XSim 脚本：

```powershell
.\scripts\run_lesson_32_vivado_xsim.ps1
```

把第 32 课文件加入 Vivado GUI 工程：

```powershell
.\scripts\run_lesson_32_add_to_vivado_project.ps1
```

## 5. 输出文件

```text
outputs/lesson_32/phase_unwrap_expected.csv
outputs/lesson_32/phase_unwrap_sim.csv
outputs/lesson_32/lesson31_phase_detrend.csv
outputs/lesson_32/phase_unwrap_detrend.png
```

其中最重要的是图：

```text
phase_unwrap_detrend.png
```

它能看到：

```text
wrapped phase 的跳变
unwrapped phase 的连续增长
第 31 课 phase 的线性拟合
去斜坡后的 residual phase
```

## 6. 对后续 FM 的意义

FM 分支后面要看的不是：

```text
载波频偏造成的线性斜坡
```

而是：

```text
FM 调制造成的周期性 phase 摆动
```

所以顺序是：

```text
phase -> unwrap -> detrend -> FM 频率/调制指数估计
```

## 7. 小检查题

### 题 1

为什么 `atan2` 输出从 `+pi` 附近跳到 `-pi` 附近时，不应该认为真实相位突然下降了？

### 题 2

如果 unwrap 后的 phase 是一条斜线，这条斜线通常说明什么？

### 题 3

为什么 FM 分析要看 detrended phase，而不是直接看原始 phase？
