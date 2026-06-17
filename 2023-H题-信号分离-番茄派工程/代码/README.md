# 代码 —— H题 番茄派信号分离(正式 CCS 工程)

> 状态:可导入 CCS 20 的正式工程,已用本机工具链(SysConfig 2.10 重生成配置 → TI Arm Clang 4.0.2 编译链接)实测通过 —— 零错误零告警,生成 `h.out`。
> 占用:Flash 约 22–24KB / 128KB;RAM 约 9.7KB / 32KB(含 C 与反馈两块 1024 点缓冲,均在静态区避开爆栈坑)。精确值见 `build/build.log`。

## 这是什么
2023 H题《信号分离装置》的番茄派 MSPM0G3507 固件工程(再生法):
`双 ADC 采集(C + 反馈) → CMSIS-DSP FFT 分析(频率/波形/相位) → AD9959 重建 A'/B' → 软件 PLL 锁相`。
针对本机 SDK 2.10 + CCS 20 + ticlang 配置好(番茄派原工程是 SDK 2.00 的,原样导入会因找不到 2.00 报错;本工程已适配 2.10)。

## 工程结构
```
代码/
├── H_SignalSep.projectspec   ← 用 CCS 导入这个(已含 SDK startup 文件)
├── H_SignalSep.syscfg        ← 片内 SYSOSC→80MHz + GPIO_DDS + SysTick (SDK 2.10)
├── main.c                    ← SYSCFG_DL_init + H_App_init + loop
├── app/                      ← H题模块: sig_acq / sig_analysis(+core) / mathacl_util / reconstruct(+core) / pll(+core) / h_app / h_config
├── User/                     ← AD9959 驱动: AD9959 / DDS / Tick
├── host_test/                ← 离板单元测试(PC/gcc): test_pll / test_sig_analysis / test_reconstruct
├── build.bat                 ← 命令行构建(可选,需英文路径)
└── README.md
```

## 方法一:CCS GUI 导入(推荐,可开发/调试/烧录)
1. CCS 20 菜单 Project → Import CCS Projects...(或 File → Import → C/C++ → CCS Projects)。
2. Select search-directory 选到本 `代码/` 文件夹 → CCS 会发现 `H_SignalSep.projectspec` 列出的工程。
3. 勾选 Copy projects into workspace → Finish。
4. 右键工程 Build;连上番茄派(XDS110/J-Link)后 Debug 即可烧录调试。
> CMSIS-DSP:本工程在链接选项里显式加了 `arm_cortexM0l_math.a`(ticlang/m0p)+ 编译宏 `-DARM_MATH_CM0`,无需在 syscfg 里再开 CMSIS 模块。
> startup:SDK 启动文件 `startup_mspm0g350x_ticlang.c` 已显式列入 projectspec(BUILD-1),不会缺 `reset/_c_int00`。

## 方法二:命令行构建(验证用)
把本 `代码/` 文件夹整体拷到纯英文路径(中文路径下 tiarmclang/sysconfig 可能报错),运行 `build.bat`:
- 自动:SysConfig(2.10)生成配置 → tiarmclang 编译 → 链接 → `build\h.out`,日志 `build\build.log`。
- 已在 `C:\hbuild\proj`(同套源码/配置)实测产出 `h.out`、零错误零告警。

## 离板单元测试(host_test,PC/gcc)
纯 C 算法核不依赖硬件/库,可在电脑上验证:
```
gcc -std=c99 -I app -o t host_test/test_pll.c          app/pll_core.c app/reconstruct_core.c -lm && ./t
gcc -std=c99 -I app -o t host_test/test_sig_analysis.c app/sig_analysis_core.c -lm && ./t
gcc -std=c99 -I app -o t host_test/test_reconstruct.c  app/reconstruct_core.c -lm && ./t
```
当前结果:test_pll 6/6(含 ppm 跟踪/相位目标/拉入/调频重锁)、test_sig_analysis 10/10(含 bin/谐波去污/归一化/反馈相位提取)、test_reconstruct 18/18。

## 模块职责(详见各 .c 注释)
| 模块 | 职责 | 关键 |
|---|---|---|
| `sig_acq` | 双 ADC 采集 | `DL_ADC12`:C→ADC0、反馈 A'+B'→ADC1;`captureDual` 近同步;软件触发(TODO TIMG 事件触发可精确锁 fs) |
| `sig_analysis`(+core) | FFT 测频/判波形/测相位 | 汉宁窗 + 块归一化 + `arm_cfft_q15`(1024)+ 3/5次谐波比 + fB=3fA 去污 + MATHACL atan2;`phasesAtBins` 提取反馈相位 |
| `mathacl_util` | 硬件数学 | MATHACL atan2/sqrt(手动上电) |
| `reconstruct`(+core) | 重建 A'/B' | AD9959 setFreq/Amp/Phase;`setFtw` 整数直写;FTW/CPOW 换算 |
| `pll`(+core) | 软件锁相 | II 型数字 PLL(int32 回绕鉴相 + int64 积分),调 FTW 锁频锁相 |
| `h_app`/`main` | 编排 | 双采集→分析→首帧建链/否则闭环→固定节拍 |

## 待办(上板相关,详见 `../上板联调清单.md`)
- `sig_acq`:软件触发 → TIMG 事件触发 + 双 ADC 同步(精确 fs + 零时间差,ACQ-7);ADC1 反馈引脚确认。
- `sig_analysis`:相位刻度上板标定(MATHACL);弱信号 SNR 阈值。
- `reconstruct`:三角波(比较器整型 + 积分器硬件,见 `../上板联调清单.md` 第1/7节);`setFtw` 字节序上板核对。
- `pll`:上板测控制周期 dt 后用 `host_test/test_pll.c` 重整定增益;接反馈通道闭合环路。

> 注:SysConfig 会提示"自动选择 MSPM0G3505"(与 G3507 同 G350X 族,寄存器兼容,无害)。
