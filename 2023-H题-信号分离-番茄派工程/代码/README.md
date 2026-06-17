# 代码 —— H题 番茄派接收识别(正式 CCS 工程)

> **状态:可导入 CCS 20 的正式工程,已用本机工具链(SysConfig 2.10 重生成配置 → TI Arm Clang 4.0.2 编译链接)实测通过 —— 零错误,生成 `h.out`。**
> 占用:Flash(text)~17.9KB / 128KB;RAM(bss)~8.8KB / 32KB(大 FFT 缓冲在静态区,避开爆栈坑)。

## 这是什么
2023 H题《信号分离装置》的番茄派 **MSPM0G3507** 固件工程(再生法):
`采集C → CMSIS-DSP FFT 分析(频率/波形/相位) → AD9959 重建 A'/B' → 软件 PLL 锁相`。
针对本机 **SDK 2.10 + CCS 20 + ticlang** 配置好(番茄派原工程是 SDK 2.00 的,原样导入会因找不到 2.00 报错;本工程已适配 2.10)。

## 工程结构
```
代码/
├── H_SignalSep.projectspec   ← 用 CCS 导入这个
├── H_SignalSep.syscfg        ← 时钟80MHz + GPIO_DDS + SysTick (SDK 2.10)
├── main.c                    ← SYSCFG_DL_init + H_App_init + loop
├── app/                      ← H题模块: sig_acq / sig_analysis / mathacl_util / reconstruct / pll / h_app / h_config
├── User/                     ← AD9959 驱动: AD9959 / DDS / Tick
├── build.bat                 ← 命令行构建(可选,需英文路径)
└── README.md
```

## 方法一:CCS GUI 导入(推荐,可开发/调试/烧录)
1. CCS 20 菜单 **Project → Import CCS Projects...**(或 File → Import → C/C++ → CCS Projects)。
2. **Select search-directory** 选到本 `代码/` 文件夹 → CCS 会发现 `H_SignalSep.projectspec` 列出的工程。
3. 勾选 **Copy projects into workspace** → Finish。
4. 右键工程 **Build**;连上番茄派(XDS110/J-Link)后 **Debug** 即可烧录调试。
> CMSIS-DSP:本工程在链接选项里显式加了 `arm_cortexM0l_math.a`(ticlang/m0p)+ 编译宏 `-DARM_MATH_CM0`,无需在 syscfg 里再开 CMSIS 模块。
> ⚠️ 若链接报"缺 reset/_c_int00":在工程里手动加入 SDK 的 `source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c`(一般 CCS 会按器件自动带上)。

## 方法二:命令行构建(验证用)
把本 `代码/` 文件夹整体拷到**纯英文路径**(中文路径下 tiarmclang/sysconfig 可能报错),运行 `build.bat`:
- 自动:SysConfig(2.10)生成配置 → tiarmclang 编译 → 链接 → `build\h.out`,日志 `build\build.log`。
- 已在 `C:\hbuild\proj`(同套源码/配置)实测产出 `h.out`、零错误。

## 模块职责(详见各 .c 注释)
| 模块 | 职责 | 关键 |
|---|---|---|
| `sig_acq` | ADC 采集 | `DL_ADC12`(ADC0);骨架软件触发,TODO TIMG+DMA→1.024MSa/s |
| `sig_analysis` | FFT 测频/判波形/测相位 | `arm_cfft_q15`(1024)+ 3次谐波比 + MATHACL atan2 |
| `mathacl_util` | 硬件数学 | MATHACL atan2/sqrt(手动上电) |
| `reconstruct` | 重建 A'/B' | AD9959 setFreq/setAmp/setPhase |
| `pll` | 软件锁相 | 定点 PI |
| `h_app`/`main` | 编排 | 采集→分析→重建→PLL |

## TODO(骨架→可跑)
- `sig_acq`:软件触发 → **TIMG 事件 + DMA** 到 1.024MSa/s
- `sig_analysis`:相位 Q 标定、波形阈值标定、加窗、谐波重叠(fB=3fA)特判
- `reconstruct`:**三角波**(比较器整型 + 积分器硬件,见 `../上板联调清单.md` §1/§7)
- `pll`:鉴相用 A'与A 真实相位差;稳态切**调 FTW 的带预调谐 PLL**(培训3 PPT)
- 相位归一化角 ↔ AD9959 14位字 精确换算

## 实测构建结果
```
SysConfig 2.10 重生成: ti_msp_dl_config / device.opt / device_linker.cmd / device.cmd.genlibs  OK
TI Arm Clang 4.0.2 编译链接: 0 error
h.out: text 17920 / data 0 / bss 8790 字节
```
> 注:SysConfig 会提示"自动选择 MSPM0G3505"(与 G3507 同 G350X 族,寄存器兼容,无害);如需强制 G3507 可在 syscfg 里改 device。
