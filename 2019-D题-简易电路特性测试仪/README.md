# 2019 国赛 D 题《简易电路特性测试仪》—— 满分国一开源工程(代码参考)

> 用途:供团队参考的 STM32F407 完整工程(2019 D 题,北邮培训题)。**代码看这里,原理图看立创(见下)。**
> 整理:2026-06-06 · 已剔除 `OBJ/` 编译产物 · 未在 Keil 里编译实测

---

## 来源 / 出处(请尊重原作者)

- 原仓库:**Gitee `kali20gakki/D-`**(标称 **满分国一**)<https://gitee.com/kali20gakki/D->
- 原仓库**无明确开源许可证**,此处仅作团队**学习参考**,版权归原作者;勿外传/商用。
- 同时保留:`原作者README.md`、`实物图.jpg`。

---

## 题目速回(2019 D 题)

被测=共射放大器(带发射极反馈+旁路)。测:输入电阻@1kHz(1k~50kΩ)、输出电阻@1kHz(500Ω~5kΩ)、增益@1kHz、幅频特性 + 上截止频率;故障判断(每项 2 秒内):电阻开/短、电容开、电容容量×2。

---

## 这份方案的架构(对应课件"方案①:单片机+运放")

```
STM32内部DAC出正弦/扫频 → 被测放大电路 → ADC采样 → CMSIS-DSP检波(幅度/相位)
   → 算 增益 / 输入输出电阻 / 幅频曲线 / 上截止频率
   → 继电器切换网络做故障判断 → ILI9341 TFT + 菜单 显示
```

- **信号源**:STM32 **内部 DAC**(软件 DDS 查表),不用外置 DDS 芯片——省器件。
- **检波**:ADC 采样 + **CMSIS-DSP(arm_/FFT)**,算幅度与相位。
- **负载切换**:继电器。
- **显示**:ILI9341 TFT + 自写菜单。

---

## 代码导读(按功能分目录,直接对题)

| 文件 / 目录 | 作用 |
|---|---|
| `USER/main.c` | 主流程 |
| `HARDWARE/DAC/bsp_dac_wave.c` | **信号源**:内部 DAC 出正弦(软件 DDS) |
| `HARDWARE/ADC/adc.c` | **检波**:ADC 采样 + CMSIS-DSP(arm_/FFT) |
| `HARDWARE/RELAY/relay.c` | **继电器**负载/网络切换 |
| `HARDWARE/TIM/tim.c` | 定时器(触发采样 / 扫频时基) |
| `HARDWARE/LCD/ili9341.c` | TFT 显示 |
| `HARDWARE/MENU/menu.c` | 菜单 UI |
| `HARDWARE/USART3/usart3.c` | 串口 |
| `HARDWARE/EXTI/exti.c` | 外部中断(按键等) |
| `DSP_LIB/` | CMSIS-DSP 头文件(FFT/数学库);**预编译 .lib(~13MB)已剔除**以精简仓库 |
| `FWLIB/` `CORE/` `SYSTEM/` | STM32F4 标准外设库 + 启动/延时/串口 |
| `USER/LED.uvprojx` | **Keil 工程入口**(名字是模板遗留的"LED",打开它即可) |

---

## ⚠️ 原理图不在本工程里

这份(以及 GitHub VxTeemo 那份)**都只有代码,没有原理图/PCB**。
**原理图 / PCB 去立创开源 000gyf 那份看**:<https://oshwhub.com/000gyf/2019d-simple-circuit-characteristic-tester>(网页用立创 EDA 直接看,含原理图+PCB+源码)。
→ **代码用这份 Gitee,图纸配立创 000gyf**,搭配起来最完整。

---

## 怎么打开 / 移植

- **打开**:Keil MDK,开 `F4_D题/USER/LED.uvprojx`,芯片 STM32F407,标准外设库。你有 STM32 经验,直接读 `HARDWARE/` 各模块即可。
- **编译注意**:为精简仓库,`DSP_LIB` 的预编译 `.lib`(~13MB)已删;要编译时在 Keil 用 RTE 勾选 CMSIS-DSP,或补回 `arm_cortexM4lf_math.lib` 即可。
- **移植到 MSPM0G3507**(可选):本方案的"内部 DAC 出正弦 + ADC 采样 + CMSIS-DSP 检波 + 继电器"——MSPM0G3507 全都有(DAC、双 ADC、可跑 CMSIS-DSP),思路可直接搬;参考仓库里 `MSPM0例程-双ADC同步采样与FFT/` 的定时触发 ADC + FFT 骨架。

---

## 另一份参考(进阶)

GitHub **VxTeemo/NUEDC2019-D**(STM32F407)是个多功能仪器大平台(AFG/示波器/频率分析/ADF4351 扫频/USB),代码全但庞杂,D 题逻辑在 `App_analysis.c`;想看 FFT 分析/扫频写法时可参考:<https://github.com/VxTeemo/NUEDC2019-D>

---

*边界:本工程为原作者代码,我只做了"挑选 + 删 OBJ + 加本导读",未在 Keil 里编译实测;复用前请自行打开构建确认。*
