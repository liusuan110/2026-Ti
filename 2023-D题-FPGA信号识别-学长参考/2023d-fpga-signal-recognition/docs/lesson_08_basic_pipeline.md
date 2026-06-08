# 2023D 第八课：基本要求总流程

前几课分别做了：

```text
AM 包络检测
AM 调制频率 F 估计
FM 相位提取
FM 的 F、m_f、Delta_f_max 估计
AM / FM / CW 自动识别
```

这一课把它们串成一个接近题目基本要求的流程：

```text
未知输入 u_M
  -> ADC 采样
  -> DDC 得到 I/Q
  -> 提取 magnitude 和 phase
  -> 识别 AM / FM / CW
  -> 按类型估计对应参数
```

## 1. 总判决逻辑

统一先算：

```text
magnitude = sqrt(I^2 + Q^2)
phase = atan2(Q, I)
```

再算两个特征：

```text
mag_depth = (mag_max - mag_min) / (mag_max + mag_min)
phase_span = (phase_max - phase_min) / 2
```

教学版判决：

```text
if mag_depth > 0.18:
    AM
elif phase_span > 0.45:
    FM
else:
    CW
```

## 2. AM 分支

如果判为 AM：

```text
m_a = mag_depth
F = 对 magnitude 做 1/2/3/4/5 kHz 候选频率匹配
```

输出：

```text
AM, F, m_a
```

## 3. FM 分支

如果判为 FM：

```text
m_f = phase_span
F = 对 phase 做 1/2/3/4/5 kHz 候选频率匹配
Delta_f_max = m_f * F
```

输出：

```text
FM, F, m_f, Delta_f_max
```

## 4. CW 分支

如果判为 CW：

```text
显示 "CW"
```

不需要估计 `F`、`m_a`、`m_f`。

## 5. 运行脚本

```powershell
cd D:\fpga\projects\02_2023_d
python .\scripts\lesson_08_basic_pipeline.py
```

输出：

```text
outputs/lesson_08/basic_pipeline_results.csv
```

## 6. 这一课的意义

这还不是最终 FPGA 代码，但它已经是一条完整算法链：

```text
分类 + 参数估计
```

后面可以做两件事：

```text
1. 加噪声、频偏、幅度变化，看看算法多稳
2. 开始把 DDC、移动平均、特征统计这些模块搬到 Verilog
```

## 7. 小变形检查题

### 题 1

为什么 AM 分支用 `magnitude` 估计 `F`，而 FM 分支用 `phase` 估计 `F`？

### 题 2

如果一个信号被判成 CW，为什么不需要估计 `m_a` 或 `m_f`？

### 题 3

这一版 Python 总流程离真实硬件还缺哪些东西？
