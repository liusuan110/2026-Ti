# 2023D 第二十八课：真正的迭代版 CORDIC atan2

上一课讲了 CORDIC 的原理。

这一课把它写成一个真正的 Verilog 模块：

```text
rtl/cordic_atan2_iterative.v
```

它的功能是：

```text
输入 I/Q
输出 phase = atan2(Q, I)
```

## 1. 为什么先写迭代复用版

完整 CORDIC 有两种常见结构：

```text
流水线版：20 级硬件，每拍可以进一个样本
迭代复用版：1 套硬件，20 拍算完一个样本
```

这一课先写迭代复用版。

原因：

```text
代码更短
更容易看懂
资源更省
适合教学和单模块验证
```

缺点：

```text
吞吐率低，大约 20 拍出一个 phase
```

以后如果真的需要每个 ADC 样本都算 phase，再改流水线版。

## 2. 模块接口

```verilog
input  wire clk
input  wire rst_n
input  wire valid_in
input  wire signed [20:0] i_in
input  wire signed [20:0] q_in

output reg  input_ready
output reg  busy
output reg  valid_out
output reg signed [23:0] phase_out
```

含义：

```text
input_ready = 1  表示模块可以接收新样本
busy = 1         表示正在迭代计算
valid_out = 1    表示 phase_out 这一拍有效
```

所以使用方式是：

```text
等 input_ready
送入 I/Q 和 valid_in
等 valid_out
读取 phase_out
```

## 3. phase 的定点格式

这一课用：

```text
Q4.20 radians
```

也就是：

```text
1 rad = 2^20
```

所以：

```text
pi ≈ 3.14159 * 2^20 ≈ 3294199
```

`phase_out` 是 24 bit signed，足够表示：

```text
-pi 到 +pi
```

## 4. 先做象限处理

为了覆盖完整 atan2 四象限，如果：

```text
I < 0
```

先把向量翻到右半平面：

```text
x = -I
y = -Q
```

同时根据 Q 的符号给初始角度：

```text
Q >= 0 -> z = +pi
Q <  0 -> z = -pi
```

如果：

```text
I >= 0
```

就：

```text
x = I
y = Q
z = 0
```

这样后面的迭代只需要负责把右半平面的向量转平。

## 5. 每一步迭代

如果：

```text
y > 0
```

说明向量还在 x 轴上方，要往下转：

```verilog
x <= x + (y >>> k);
y <= y - (x >>> k);
z <= z + atan(2^-k);
```

如果：

```text
y < 0
```

说明向量在 x 轴下方，要往上转：

```verilog
x <= x - (y >>> k);
y <= y + (x >>> k);
z <= z - atan(2^-k);
```

这里：

```text
>>> 是算术右移
```

对 signed 数很重要。

## 6. 为什么 y=0 可以提前结束

如果：

```text
y == 0
```

说明向量已经在 x 轴上。

这时：

```text
z 就是当前估计角度
```

模块会直接输出结果。

这也让一些特殊点更干净：

```text
(I,Q) = (1,1) 一步就得到 45 度
(I,Q) = (-1,0) 直接得到 pi
```

## 7. 测试脚本

文件：

```text
scripts/lesson_28_prepare_and_check_cordic.py
```

它会生成：

```text
sim/cordic_i_81.mem
sim/cordic_q_81.mem
outputs/lesson_28/cordic_atan2_expected.csv
outputs/lesson_28/cordic_atan2_iterative_expected.png
```

运行：

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_28_prepare_and_check_cordic.py
```

## 8. 仿真文件

文件：

```text
sim/tb_cordic_atan2_iterative.v
```

如果以后装好了 Icarus Verilog，可以运行：

```powershell
iverilog -o outputs\lesson_28\tb_cordic_atan2_iterative.vvp rtl\cordic_atan2_iterative.v sim\tb_cordic_atan2_iterative.v
vvp outputs\lesson_28\tb_cordic_atan2_iterative.vvp
python .\scripts\lesson_28_prepare_and_check_cordic.py
```

最后一步会比较 Verilog 仿真输出和 Python 期望输出。

## 9. 这一课要记住

```text
迭代版 CORDIC = 一套加减移位硬件 + step 计数器。
```

它不是外接模块，也不是高级函数。

它就是 FPGA 里一拍一拍做：

```text
判断 y 的符号
做一次小旋转
累计 z
重复 20 次
```

最后输出：

```text
phase_out ≈ atan2(Q,I)
```

## 10. 小变形检查题

### 题 1

`input_ready` 和 `valid_out` 分别表示什么？

### 题 2

为什么 `phase_out` 用 Q4.20 这样的定点数，而不是直接用浮点数？

### 题 3

为什么这个迭代版 CORDIC 暂时不能每个采样时钟都接收一个新 I/Q？
