# 2023D 第十四课：FPGA 里一般怎么得到 cos

这个问题很关键：

```text
一般使用什么公式来求 cos？
```

答案是：

```text
DDS 运行时通常不在 FPGA 里“求 cos”。
它通常是提前算好一张 cos 表，FPGA 运行时只查表。
```

这叫：

```text
LUT = Look-Up Table
查找表
```

## 1. 先修正一个常见理解

“用大数 1 / 大数 2 模拟 0.xx 的小数”，这个方向对。

但要分清两件事：

```text
phase_step 控制输出频率
PHASE_BITS 控制频率分辨率
```

不是：

```text
phase_step 越大越粗糙
```

而是：

```text
phase_step 越大 -> 每个时钟相位走得越多 -> 输出频率越高
```

频率能调得多细，主要看分母有多大：

```text
2^PHASE_BITS
```

所以：

```text
PHASE_BITS 越多 -> 频率步进越细
LUT_ADDR_BITS 越多 -> 波形表越细
AMP_BITS 越多 -> 幅度量化越细
```

这三个“精度”不是同一个东西。

## 2. 三种常见方法

### 方法 A：查表 LUT

这是 DDS 里最常见、最适合入门的方法。

离线算：

```text
cos(2*pi*k/N), k = 0, 1, 2, ..., N-1
```

然后量化成整数，存入 ROM。

FPGA 运行时：

```text
addr = phase 的高位
cos_value = cos_rom[addr]
```

优点：

```text
快
结构简单
每个时钟稳定输出
容易仿真
```

缺点：

```text
需要 ROM 资源
表越大，占资源越多
```

### 方法 B：CORDIC

CORDIC 可以用加法、移位、查小表来算：

```text
sin/cos
幅度
相位 atan2
```

它很适合 FPGA，但比 LUT 难一点。

我们后面做 `atan2(Q, I)` 或 phase 时，很可能会再见到 CORDIC。

### 方法 C：多项式 / 泰勒展开

理论上可以用：

```text
cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + ...
```

但是在 FPGA 的 DDS 里，它不是最常用的入门方案。

原因是：

```text
要做范围规约
要做多次乘法
要处理误差
流水线更复杂
```

所以“泰勒展开”是数学上可行的，但工程上 DDS 入门通常不用它。

## 3. LUT 的表值从哪里来

表值通常由软件提前生成。

例如 Python：

```python
code[k] = round(cos(2*pi*k/N) * 2047)
```

这里 `2047` 来自 12 bit 有符号数：

```text
最大正数 = 2^(12-1) - 1 = 2047
```

所以 ROM 里存的不是小数，而是整数：

```text
cos = 1      -> 2047
cos = 0      -> 0
cos = -1     -> -2047
```

FPGA 里拿到的是这些整数码值。

## 4. 为什么用 phase 高位查表

假设：

```text
PHASE_BITS = 32
LUT_ADDR_BITS = 10
```

phase 是 32 bit，很细。

ROM 只有：

```text
2^10 = 1024
```

个点。

所以用高 10 位做地址：

```text
addr = phase[31:22]
```

低 22 位相当于小数细节，被截掉。

这就是：

```text
相位截断
```

它会带来一点误差，但结构很简单。

## 5. 四分之一波表

cos 有对称性。

其实只存 0 到 90 度也可以：

```text
0 到 90 度：直接查
90 到 180 度：镜像 + 取负
180 到 270 度：直接查 + 取负
270 到 360 度：镜像
```

这样 ROM 大约可以省到四分之一。

但控制逻辑稍微复杂一点。

入门时可以先用完整一圈表：

```text
1024 点完整 cos 表
```

等 DDS 跑通后，再考虑省 ROM。

## 6. 本课脚本做了什么

脚本会生成：

```text
1024 点
12 bit 幅度
cos LUT
```

并输出：

```text
CSV 表格
COE 初始化文件
PNG 误差图
```

其中 COE 文件以后可以给 Vivado Block Memory Generator 使用。

## 7. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_14_cos_lut_generation.py
```

输出：

```text
outputs/lesson_14/cos_lut_samples.csv
outputs/lesson_14/cos_full_wave_1024x12.coe
outputs/lesson_14/cos_full_wave_1024x12_hex.coe
outputs/lesson_14/cos_lut_generation.png
```

## 8. 这一课要记住

```text
FPGA 里的 DDS 通常不是运行时求 cos。
它是离线算好 cos 表，运行时用 phase 查 ROM。
```

再压缩成一句：

```text
软件负责算表，硬件负责查表。
```

## 9. 小变形检查题

### 题 1

如果 `LUT_ADDR_BITS` 从 10 增加到 12，ROM 点数从多少变成多少？

### 题 2

`AMP_BITS` 增加，主要改善的是频率精度，还是幅度量化误差？

### 题 3

为什么 DDS 入门更常用 LUT，而不是直接在 FPGA 里写泰勒展开？
