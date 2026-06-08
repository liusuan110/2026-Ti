# 2023D 第三十四课：硬件版 phase 斜率估计

第 33 课已经有：

```text
DDC -> CORDIC -> unwrap -> unwrapped phase
```

下一步要从 `unwrapped phase` 里估计载波频偏造成的斜率。

## 1. 为什么先估斜率

如果真实载波是：

```text
fc + Delta_f
```

DDC 本振用的是：

```text
fc
```

那么 unwrap 后的 phase 近似是：

```text
phase[n] = phase0 + slope * n + 调制相位
```

其中：

```text
slope = 2*pi*Delta_f / Fs
```

所以先估 `slope`，就能估载波频偏。

## 2. 为什么不用 polyfit

Python 里可以直接：

```python
np.polyfit(...)
```

但 FPGA 里不能这么写。

本课先用最简单的端点法：

```text
delta = phase_last - phase_first
slope = delta / (N - 1)
```

为了让硬件简单，我们选：

```text
N = 8193
N - 1 = 8192 = 2^13
```

所以：

```text
slope = delta >>> 13
```

除法就变成了右移。

## 3. 新模块

文件：

```text
rtl/phase_slope_estimator.v
```

输入：

```text
valid_in
phase_unwrapped_in
```

输出：

```text
estimate_valid
delta_phase_out
slope_per_sample_out
```

调试输出：

```text
first_phase_debug
last_phase_debug
window_sample_count_debug
```

## 4. 用第 33 课数据验证

脚本读取：

```text
outputs/lesson_33/ddc_phase_unwrap_core_expected.csv
```

里面是 `+5000 Hz` 载波频偏的 unwrapped phase。

端点法估计结果：

```text
block 0 -> 约 5042 Hz
block 1 -> 约 4960 Hz
```

它们围绕真实 `5000 Hz` 摆动。

这说明：

```text
端点法能抓住趋势，但还不是最终高精度方案。
```

误差来自 DDC 残余纹波、量化和端点位置。

## 5. 运行

跑 Vivado/XSim：

```powershell
.\scripts\run_lesson_34_vivado_xsim.ps1
```

加入 Vivado GUI 工程：

```powershell
.\scripts\run_lesson_34_add_to_vivado_project.ps1
```

## 6. 输出文件

```text
outputs/lesson_34/phase_slope_estimator_expected.csv
outputs/lesson_34/phase_slope_estimator_sim.csv
outputs/lesson_34/phase_slope_estimator_expected.png
```

## 7. 这一课要记住

本课不是最终 detrend。

它解决的是第一步：

```text
怎么在硬件里估出 phase 的线性斜率
```

后面会继续做：

```text
phase_detrended[n] = phase_unwrapped[n] - slope * n
```

## 8. 小检查题

### 题 1

为什么窗口长度选 `8193`，而不是随便选一个数字？

### 题 2

为什么端点法估计 5000 Hz 时会有几十 Hz 的摆动？

### 题 3

本课输出的 `slope_per_sample_out` 表示“每秒变化多少相位”还是“每个样本变化多少相位”？
