@echo off
setlocal
rem ======================================================================
rem  2023 H题 信号分离 — 命令行构建 (CCS GUI 之外的构建/验证)
rem  注意: 路径含中文时 tiarmclang/sysconfig 可能报错 ->
rem        请先把本工程文件夹整体拷到纯英文路径 (如 C:\hbuild\proj) 再运行本脚本.
rem  正常开发请用 CCS GUI: 导入 H_SignalSep.projectspec (见 README).
rem ======================================================================
set CGT=C:\ti\ti_cgt_arm_llvm_4.0.2.LTS
set SDK=C:\ti\mspm0_sdk_2_10_00_04
set SYSCFG=C:\ti\sysconfig_1.26.2\sysconfig_cli.bat
set CMSIS=%SDK%\source\third_party\CMSIS
set PROJ=%~dp0
set OUT=%PROJ%build
set LOG=%OUT%\build.log
set CLANG="%CGT%\bin\tiarmclang.exe"
set FLAGS=-march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -gdwarf-3 -D__MSPM0G3507__ -DARM_MATH_CM0

if not exist "%OUT%\syscfg" mkdir "%OUT%\syscfg"
set INCS=-I"%PROJ%" -I"%PROJ%app" -I"%PROJ%User" -I"%OUT%\syscfg" -I"%CMSIS%\Core\Include" -I"%CMSIS%\DSP\Include" -I"%SDK%\source"

echo ===== SYSCONFIG (2.10) ===== > "%LOG%"
call "%SYSCFG%" --compiler ticlang --product "%SDK%\.metadata\product.json" --output "%OUT%\syscfg" "%PROJ%H_SignalSep.syscfg" >> "%LOG%" 2>&1

cd /d "%OUT%"
if exist h.out del h.out
echo ===== COMPILE ===== >> "%LOG%"
%CLANG% -c %FLAGS% %INCS% -o main.o         "%PROJ%main.c"            >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o AD9959.o       "%PROJ%User\AD9959.c"     >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o DDS.o          "%PROJ%User\DDS.c"        >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o Tick.o         "%PROJ%User\Tick.c"       >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o UserTask.o     "%PROJ%User\UserTask.c"   >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o mathacl_util.o "%PROJ%app\mathacl_util.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o sig_acq.o      "%PROJ%app\sig_acq.c"     >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o sig_analysis.o "%PROJ%app\sig_analysis.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o sig_analysis_core.o "%PROJ%app\sig_analysis_core.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o reconstruct.o  "%PROJ%app\reconstruct.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o reconstruct_core.o "%PROJ%app\reconstruct_core.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o pll.o          "%PROJ%app\pll.c"         >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o h_app.o        "%PROJ%app\h_app.c"       >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o syscfg_config.o "%OUT%\syscfg\ti_msp_dl_config.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o startup.o "%SDK%\source\ti\devices\msp\m0p\startup_system_files\ticlang\startup_mspm0g350x_ticlang.c" >> "%LOG%" 2>&1

echo ===== LINK ===== >> "%LOG%"
%CLANG% %FLAGS% -Wl,-m"h.map" -Wl,-i"%SDK%\source" -Wl,-i"%OUT%\syscfg" -Wl,-i"%CGT%\lib" -Wl,--rom_model -o "h.out" main.o AD9959.o DDS.o Tick.o UserTask.o mathacl_util.o sig_acq.o sig_analysis.o sig_analysis_core.o reconstruct.o reconstruct_core.o pll.o h_app.o syscfg_config.o startup.o -Wl,-l"syscfg\device_linker.cmd" -Wl,-ldevice.cmd.genlibs "%CMSIS%\DSP\lib\ticlang\m0p\arm_cortexM0l_math.a" -Wl,-llibc.a >> "%LOG%" 2>&1
echo ===== RESULT ===== >> "%LOG%"
dir h.out >> "%LOG%" 2>&1
echo BAT_DONE
