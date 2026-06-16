# 代码 —— H题 番茄派接收识别(架构骨架)

> **状态:架构搭建完成,已用 CCS 工具链(TI Arm Clang 4.0.2 + MSPM0 SDK 2.10)实测编译链接通过 —— 零错误、零警告,生成可烧录镜像 `h.out`。**
> 占用:**Flash(text)17.9KB / 128KB(14%)**;**RAM(bss)8.8KB / 32KB(27%)**。大 FFT 缓冲全部放静态区,**避开了 SHR 工程"栈上放 FFT 数组导致爆栈"的坑**。

## 方案:再生法
`采集C → FFT分析(频率/波形/相位) → AD9959重建A'/B' → 软件PLL闭环锁相`

## 模块清单(app/)
| 文件 | 职责 | 关键实现 |
|---|---|---|
| `h_config.h` | 全局参数 | fs=1.024MSa/s、N=1024、fRes=1kHz、频点10–100k/5k、误差目标 |
| `sig_acq.c/.h` | ADC 采集 | `DL_ADC12`(ADC0);**骨架=软件触发轮询**,TODO 升级 TIMG事件+DMA 到 1.024MSa/s |
| `sig_analysis.c/.h` | 信号分析 | **CMSIS-DSP 定点 `arm_cfft_q15`(1024点)**→ 寻两峰→频率/幅度;3次谐波能量比判正弦/三角;`MathUtil_atan2` 求相位 |
| `mathacl_util.c/.h` | 硬件数学 | **MATHACL** `atan2/sqrt` 封装(.syscfg 未配,手动 reset+enablePower) |
| `reconstruct.c/.h` | 重建输出 | 调 `AD9959_setFreq/setAmp/setPhase` 重画 A'/B' |
| `pll.c/.h` | 软件锁相 | 定点 PI 环路(抗独立晶振漂移;预调谐/按频率缩放留接口) |
| `h_app.c/.h` | 编排 | 采集→分析→重建→PLL闭环;固定控制周期 |
| `main.c` | 入口 | `SYSCFG_DL_init()`(80MHz+GPIO_DDS+SysTick)→ `H_App_init` → loop |

## 怎么编译(已验证的命令行流程)
底座 = 仓库的 `AD9959-DDS信号源资料/番茄派例程-MSPM0G3507_AD9959-学长培训/`(需带 `Debug/syscfg/` 的 SysConfig 生成物;`培训资料/AD9959控制例程.zip` 里是完整带 Debug 的版本)。
1. 把底座工程拷到**纯英文路径**(如 `C:\hbuild\proj`)。
2. 本目录 `app/` 放进 `proj\app\`;用 `main.c` 覆盖 `proj\MSPM0G3507_AD9959DDSCtrl_Demo.c`。
3. 跑 `build.bat`(tiarmclang 编译+链接 → `h.out`,日志 `build.log`)。

**关键编译要素**(build.bat 已内置):
- 器件宏 `-D__MSPM0G3507__`;CMSIS 定点宏 **`-DARM_MATH_CM0`**(注意是 CM0 不是 CM0P)
- include:`SDK\source`、`CMSIS\Core\Include`、`CMSIS\DSP\Include`、`proj\app`、`proj\Debug\syscfg`
- 链接:`driverlib.a`(经 `device.cmd.genlibs`)+ **CMSIS-DSP 预编译库 `CMSIS\DSP\lib\ticlang\m0p\arm_cortexM0l_math.a`** + `device_linker.cmd`
- 内核:`-march=thumbv6m -mcpu=cortex-m0plus -mthumb -mfloat-abi=soft`

## CCS GUI 集成(下一步,要图形化开发时)
导入底座 CCS 工程 → 把 `app/` 加入工程 → 工程属性里:加 CMSIS 两个 include 路径 + 预定义 `ARM_MATH_CM0` + 链接 `arm_cortexM0l_math.a`。(本目录已用**同一套工具链**命令行验证可编过。)

## 已知 TODO(骨架待充实 → 真正能跑)
- **sig_acq**:软件触发 → **TIMG 事件 + DMA**,稳定到 1.024MSa/s(侦察已给确切 API)
- **sig_analysis**:相位 Q 格式标定、波形判别阈值标定、加窗、**谐波重叠(fB=3·fA)特判**(参考 SHR 精读)
- **reconstruct**:三角波(AD9959 出正弦 → **比较器整型 + 积分器**硬件,见 `../硬件/README`)
- **pll**:鉴相改为 **A'与A 的真实相位差**;稳态后切到**调 FTW 的带预调谐 PLL**(`FTW=预设+环路输出`)、PI 随频率缩放、控制周期固定
- **相位换算**:MATHACL 归一化角 ↔ AD9959 14位相位字 的精确标定

## 实测构建结果
```
   text    data     bss     dec     hex   filename
  17920       0    8790   26710    6856   h.out      (TI Arm Clang 4.0.2, SDK 2.10)
```
