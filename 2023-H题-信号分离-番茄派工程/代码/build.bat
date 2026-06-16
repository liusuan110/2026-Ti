@echo off
setlocal
set CGT=C:\ti\ti_cgt_arm_llvm_4.0.2.LTS
set SDK=C:\ti\mspm0_sdk_2_10_00_04
set PROJ=C:\hbuild\proj
set CMSIS=%SDK%\source\third_party\CMSIS
set LOG=C:\hbuild\build.log
set CLANG="%CGT%\bin\tiarmclang.exe"
set FLAGS=-march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -gdwarf-3 -D__MSPM0G3507__ -DARM_MATH_CM0
set INCS=-I"%PROJ%" -I"%PROJ%\User" -I"%PROJ%\app" -I"%PROJ%\Debug\syscfg" -I"%CMSIS%\Core\Include" -I"%CMSIS%\DSP\Include" -I"%SDK%\source"
cd /d "%PROJ%\Debug"
if exist h.out del h.out
echo ===== VERSION ===== > "%LOG%"
%CLANG% --version >> "%LOG%" 2>&1
echo ===== COMPILE ===== >> "%LOG%"
%CLANG% -c %FLAGS% %INCS% -o main.o        "..\MSPM0G3507_AD9959DDSCtrl_Demo.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o AD9959.o      "..\User\AD9959.c"   >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o DDS.o         "..\User\DDS.c"      >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o Tick.o        "..\User\Tick.c"     >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o UserTask.o    "..\User\UserTask.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o mathacl_util.o "..\app\mathacl_util.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o sig_acq.o     "..\app\sig_acq.c"      >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o sig_analysis.o "..\app\sig_analysis.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o reconstruct.o "..\app\reconstruct.c"  >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o pll.o         "..\app\pll.c"          >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o h_app.o       "..\app\h_app.c"        >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o syscfg_config.o "syscfg\ti_msp_dl_config.c" >> "%LOG%" 2>&1
%CLANG% -c %FLAGS% %INCS% -o startup.o "%SDK%\source\ti\devices\msp\m0p\startup_system_files\ticlang\startup_mspm0g350x_ticlang.c" >> "%LOG%" 2>&1
echo ===== LINK ===== >> "%LOG%"
%CLANG% %FLAGS% -Wl,-m"h.map" -Wl,-i"%SDK%\source" -Wl,-i"%PROJ%\Debug\syscfg" -Wl,-i"%CGT%\lib" -Wl,--rom_model -o "h.out" main.o AD9959.o DDS.o Tick.o UserTask.o mathacl_util.o sig_acq.o sig_analysis.o reconstruct.o pll.o h_app.o syscfg_config.o startup.o -Wl,-l"syscfg\device_linker.cmd" -Wl,-ldevice.cmd.genlibs "%CMSIS%\DSP\lib\ticlang\m0p\arm_cortexM0l_math.a" -Wl,-llibc.a >> "%LOG%" 2>&1
echo ===== RESULT ===== >> "%LOG%"
dir "h.out" >> "%LOG%" 2>&1
echo BAT_DONE
