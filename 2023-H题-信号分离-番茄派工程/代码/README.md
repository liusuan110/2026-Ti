# 代码 —— H题 番茄派信号分离(正式 CCS 工程)

> 状态:可导入 CCS 20 的正式工程,已用本机工具链(SysConfig 2.10 重生成配置 → TI Arm Clang 4.0.2 编译链接)实测通过 —— 零错误零告警(projectspec 用 `-Wall`;另以 `-Wall -Wextra` 手工编译 app 各模块、阶段 1/2/3/4 + 软件触发回退 + 屏波形/触摸开关各组合均无告警),生成 `h.out`。
> 占用:Flash 约 35KB / 128KB;RAM 约 10.6KB(数字PLL)/ 8.7KB(模拟PID,省了反馈缓冲)/ 32KB。精确值见本地跑 `build.bat` 后生成的 `build/build.log`(构建产物,未入库)或 CCS 的 .map。
> ★设备容量:目标 MSPM0G3507 为 **128KB Flash / 32KB SRAM**。裸 SysConfig CLI 会把 G350X 家族默认成 G3505(32KB/16KB),故 `build.bat` 已自动把生成的 `device_linker.cmd` 内存图修正回 128KB/32KB;CCS 经 projectspec 的 `device=MSPM0G3507` 本就是 128KB。若 CCS 报 "won't fit into available memory",检查工程属性 Device 是否为 MSPM0G3507(而非 G3505)。

## 这是什么
2023 H题《信号分离装置》的番茄派 MSPM0G3507 固件工程(再生法):
`ADC【定时器】采集 C → CMSIS-DSP FFT 分析(频率/波形/相位, 频率吸附5kHz网格) → AD9959 重建 A'/B' → 锁相同频显示`,
带 **串口遥测 + 分阶段 bring-up**(上板可观测)+ **淘晶驰串口屏**(TJC4827X243_011C:结果/波形/触摸/多页)。见 `app/h_config.h` 开关与 `../上板联调清单.md`。

**锁相方式两条路线(`H_LOCK_METHOD` 开关二选一,上板可 A/B 对比):**
- `H_LOCK_DIGITAL_PLL`(替代,已离板验证):第2路 ADC 同步采 A'+B' → FFT → atan2 提相位 → II 型数字 PLL 调 FTW。纯数字。
- `H_LOCK_ANALOG_PID`(主线·默认,学长思路):外部模拟乘法器(原信号×AD9959正交本振)→ 直流电压(=相位误差),ADC1 读 → float PID 调频率正交锁定。固件更简单(无双ADC同步/无第二次FFT/无atan2),复杂度在模拟硬件。AD9959 用 4 通道(CH0/1 输出 + CH2/3 正交本振)。

针对本机 SDK 2.10 + CCS 20 + ticlang 配置好(番茄派原工程是 SDK 2.00 的,原样导入会报错;本工程已适配 2.10)。

## 工程结构
```
代码/
├── H_SignalSep.projectspec   ← 用 CCS 导入这个(已含 SDK startup 文件 + 全部源文件)
├── H_SignalSep.syscfg        ← 片内 SYSOSC→80MHz + GPIO_DDS + SysTick + UART_TEL(遥测) + UART_TJC(串口屏) + TIMER_ADC(ADC触发)
├── main.c                    ← SYSCFG_DL_init + H_App_init + loop
├── app/                      ← H题模块: sig_acq / sig_analysis(+core) / mathacl_util / reconstruct(+core) / pll(+core) / pid_core / telemetry(+core) / tjc_hmi(+core) / h_app / h_config
├── User/                     ← AD9959 驱动: AD9959 / DDS / Tick
├── host_test/                ← 离板单元测试(PC/gcc): test_pll / test_sig_analysis / test_reconstruct / test_telemetry / test_tjc_hmi / test_pid
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
gcc -std=c99 -I app -o t host_test/test_telemetry.c    app/telemetry_core.c        && ./t
gcc -std=c99 -I app -o t host_test/test_tjc_hmi.c      app/tjc_hmi_core.c app/telemetry_core.c && ./t
gcc -std=c99 -I app -o t host_test/test_pid.c          app/pid_core.c -lm && ./t
```
当前结果(6 套):test_pll 6/6、test_sig_analysis 11/11(bin/谐波去污/归一化/**5kHz网格吸附**/反馈相位提取)、test_reconstruct 18/18、test_telemetry 31/31、test_tjc_hmi **46/46**(指令字节/解析状态机:触摸0x65·页0x66·相位帧·addt握手0xFE/0xFD·噪声重同步)、test_pid **6/6**(比例/输出限幅/抗积分饱和/**闭环收敛消扰动**/dt=0安全)。
> 中文路径下 gcc 可能因编码报错 —— 把 `app/` 与 `host_test/` 拷到纯英文路径再编译。

## 模块职责(详见各 .c 注释)
| 模块 | 职责 | 关键 |
|---|---|---|
| `sig_acq` | ADC 采集 | `DL_ADC12`:C→ADC0(定时器 TIMA0 触发, fs 稳定);ADC1 按锁相方式切:**数字PLL**=块采反馈 A'+B'(与ADC0同步);**模拟PID**=`initPD/readPD` 2 通道单次读鉴相电压。采集硬超时防卡死 |
| `sig_analysis`(+core) | FFT 测频/判波形/测相位 | 汉宁窗 + 块归一化 + `arm_cfft_q15`(1024)+ 3/5次谐波比 + fB=3fA 去污 + MATHACL atan2;**`snapFreq` 频率吸附5kHz网格**;`phasesAtBins` 提取反馈相位(数字PLL用) |
| `mathacl_util` | 硬件数学 | MATHACL atan2/sqrt(手动上电) |
| `reconstruct`(+core) | 重建 A'/B' | AD9959 setFreq/Amp/Phase;`setFtw` 整数直写;FTW/CPOW 换算;**模拟模式 `setAnalog` 驱动 4 通道**(CH0/1输出 + CH2/3正交本振) |
| `pll`(+core) | **数字锁相**(法一) | II 型数字 PLL(int32 回绕鉴相 + int64 积分),调 FTW 锁频锁相;`Pll_diag` 供遥测读 perr/ftw/locked |
| `pid_core` | **模拟鉴相控制**(法二) | 纯 float 增量式 PID(抗积分饱和+限幅, dt入参);反馈=鉴相电压→输出微调频率正交锁定;增益按频率调度。离板可单测 |
| `telemetry`(+core) | 串口遥测 | 每周期(抽稀)经 UART0 打印 ADC统计/频率/波形/相位/PLL状态;`telemetry_core` 纯C格式化可离板单测 |
| `tjc_hmi`(+core) | 淘晶驰串口屏 | UART1 双向:结果页(频率/波形/幅度/相位差/锁定,只发变化字段)+ **实时波形页**(C 抽取缩放→`addt`透传到 s0,带就绪超时防卡死)+ **触摸调相位**(屏→MCU 解析状态机→s_target)+ **多页**(sendme 跟踪当前页);全部指令构建/解析进 core 可离板单测 |
| `h_app`/`main` | 编排 | 双采集→分析→建链/闭环→**遥测+屏幕**→固定节拍;**分阶段 bring-up**(`H_STAGE` 1采集/2分析/3开环/4闭环) |

## 待办(上板相关,详见 `../上板联调清单.md`)
- ✅ 已完成(离板已验证,等上板):定时器双 ADC 同步触发 + fs 稳定;5kHz 网格吸附;串口遥测 + 分阶段 bring-up;采集超时。
- `sig_acq`:ADC1 反馈引脚确认;定时器事件路由/双路块对齐上板示波器实测(代码已按"停表→两路使能→启表"消除错位竞态)。
- `sig_analysis`:相位刻度上板标定(MATHACL);弱信号 SNR 阈值(可在 `result_ok` 加幅度门限)。
- `reconstruct`:三角波(比较器整型 + 积分器硬件,见 `../上板联调清单.md` 第1/7节);`setFtw` 字节序上板核对。
- `pll`:上板测控制周期 dt(遥测可读帧间隔)后用 `host_test/test_pll.c` 重整定增益;接反馈通道闭合环路。

> 注:SysConfig 会提示"自动选择 MSPM0G3505"(与 G3507 同 G350X 族,寄存器兼容,无害)。
