# 2023D 第十五课：DDS/NCO 的 Verilog 雏形

这一课开始写第一个真正的 FPGA 小模块：

```text
dds_nco
```

它暂时不接 ADC，也不做 DDC。它只负责一件事：

```text
每个采样时钟输出一组 2 MHz 的 cos/sin 数字样本。
```

## 1. 模块接口

文件：

```text
rtl/dds_nco.v
```

接口是：

```verilog
input  wire clk
input  wire rst_n
input  wire valid_in
output reg  valid_out
output reg signed [11:0] cos_out
output reg signed [11:0] sin_out
output reg [31:0] phase_out
```

含义：

```text
clk       采样时钟，比如 20 MHz
rst_n     低有效复位
valid_in  本拍是否要产生一个新采样
valid_out 输出是否有效
cos_out   cos 查表输出
sin_out   sin 查表输出
phase_out 当前相位值，方便仿真观察
```

## 2. 最核心的一行

DDS 的核心仍然是：

```verilog
phase_acc <= phase_acc + PHASE_STEP;
```

这行只有在 `valid_in = 1` 时执行。

所以如果未来 ADC 不是每个系统时钟都给一个采样，就可以用 `valid_in` 控制 DDS 跟着采样节奏走。

## 3. 查表地址

我们用：

```text
PHASE_BITS = 32
LUT_ADDR_BITS = 10
```

所以 ROM 有：

```text
2^10 = 1024
```

个点。

查表地址来自相位高位：

```verilog
assign lut_addr = phase_acc[PHASE_BITS-1:ADDR_LSB];
```

其中：

```text
ADDR_LSB = PHASE_BITS - LUT_ADDR_BITS
         = 22
```

也就是：

```text
lut_addr = phase_acc[31:22]
```

## 4. ROM 表从哪里来

这一课用脚本生成两个 ROM 初始化文件：

```text
rtl/cos_1024x12.mem
rtl/sin_1024x12.mem
```

脚本是：

```text
scripts/lesson_15_prepare_and_check_dds.py
```

它会把 cos/sin 量化成 12 bit 二进制补码十六进制。

Verilog 里用：

```verilog
$readmemh(COS_MEM_FILE, cos_rom);
$readmemh(SIN_MEM_FILE, sin_rom);
```

把表读进 ROM。

## 5. 为什么输出第一拍是 cos=2047, sin=0

复位后：

```text
phase_acc = 0
```

所以：

```text
lut_addr = 0
```

查表得到：

```text
cos(0) = 1      -> 2047
sin(0) = 0      -> 0
```

所以第一拍输出：

```text
cos_out = 2047
sin_out = 0
```

下一拍 phase 加上 `PHASE_STEP`，地址前进，输出下一个相位点。

## 6. 运行准备脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_15_prepare_and_check_dds.py
```

它会生成：

```text
rtl/cos_1024x12.mem
rtl/sin_1024x12.mem
outputs/lesson_15/dds_nco_expected.csv
```

## 7. 仿真文件

文件：

```text
sim/tb_dds_nco.v
```

它会：

```text
产生 20 MHz 时钟
复位 DDS
连续取 80 个输出采样
写入 outputs/lesson_15/dds_nco_sim.csv
```

如果以后使用 Icarus Verilog，可以运行：

```powershell
iverilog -o outputs\lesson_15\tb_dds_nco.vvp rtl\dds_nco.v sim\tb_dds_nco.v
vvp outputs\lesson_15\tb_dds_nco.vvp
python .\scripts\lesson_15_prepare_and_check_dds.py
```

最后一个 Python 命令会检查仿真 CSV 是否和期望值一致。

## 8. 这一课要记住

```text
Verilog 里的 DDS 就是：
一个相位寄存器
一个固定 phase_step
两个 ROM 表
每个有效采样时钟输出一个 cos/sin
```

先不要急着接后面的 DDC。

真正的 FPGA 学习要一块一块确认：

```text
DDS 正确
再接 Mixer
再接低通
再接分类器
```

## 9. 小变形检查题

### 题 1

为什么 `phase_acc` 只在 `valid_in = 1` 时增加？

### 题 2

如果第一拍 `phase_acc = 0`，为什么 `cos_out` 应该接近最大正数？

### 题 3

`lut_addr = phase_acc[31:22]` 这句话是在取相位的高位还是低位？为什么取这些位？
