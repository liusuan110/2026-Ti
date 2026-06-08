# 2023D 第三十五课：平均端点法估计 phase 斜率

第 34 课用的是最简单的端点法：

```text
slope = (phase_last - phase_first) / 8192
```

问题是它只看两个点。

如果这两个点刚好落在 DDC 残余纹波的高点或低点，估计值就会抖。

## 1. 本课改进

本课不用单个端点，而是用小窗口平均：

```text
start_sum = 前 16 个 phase 求和
end_sum   = 隔 8192 个样本后的 16 个 phase 求和
```

然后：

```text
slope = (end_sum - start_sum) / (16 * 8192)
```

因为：

```text
16 * 8192 = 2^4 * 2^13 = 2^17
```

所以硬件里仍然只是：

```text
slope = delta_sum >>> 17
```

## 2. 为什么这样更稳

第 34 课：

```text
只让 2 个样本决定 slope
```

第 35 课：

```text
让前 16 个样本和后 16 个样本共同决定 slope
```

残余纹波和量化误差会在求和时部分抵消。

## 3. 新模块

文件：

```text
rtl/phase_slope_mean_estimator.v
```

输入：

```text
valid_in
phase_unwrapped_in
```

输出：

```text
estimate_valid
delta_sum_out
slope_per_sample_out
```

调试输出：

```text
start_sum_debug
end_sum_debug
block_sample_count_debug
```

## 4. 和第 34 课对比

同样使用第 33 课的 `+5000 Hz` 频偏数据。

第 34 课端点法：

```text
block 0 -> 5042 Hz
block 1 -> 4960 Hz
```

第 35 课平均端点法：

```text
block 0 -> 4999.7 Hz
block 1 -> 4996.7 Hz
```

可以看到稳定很多。

## 5. 运行

跑 Vivado/XSim：

```powershell
.\scripts\run_lesson_35_vivado_xsim.ps1
```

加入 Vivado GUI 工程：

```powershell
.\scripts\run_lesson_35_add_to_vivado_project.ps1
```

## 6. 输出文件

```text
outputs/lesson_35/phase_slope_mean_estimator_expected.csv
outputs/lesson_35/phase_slope_mean_estimator_sim.csv
outputs/lesson_35/phase_slope_mean_estimator_expected.png
```

## 7. 这一课要记住

平均端点法的核心思想是：

```text
用多一点样本，换更稳定的 slope
```

它仍然比最小二乘简单很多，但已经比单端点法可靠。

## 8. 小检查题

### 题 1

为什么本课用 `16` 个样本做平均，而不是随便取 `13` 个？

### 题 2

为什么 `end_sum - start_sum` 之后要右移 `17` 位？

### 题 3

平均端点法比单端点法多用了哪些硬件资源？
