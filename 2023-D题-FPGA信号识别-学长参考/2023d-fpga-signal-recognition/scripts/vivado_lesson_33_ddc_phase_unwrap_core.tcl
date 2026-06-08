set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs lesson_33]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Lesson 33: compile DDC phase unwrap core with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl dds_nco.v] \
    [file join $root_dir rtl ddc_mixer.v] \
    [file join $root_dir rtl moving_average.v] \
    [file join $root_dir rtl ddc_core.v] \
    [file join $root_dir rtl cordic_atan2_pipeline.v] \
    [file join $root_dir rtl ddc_phase_core_pipeline.v] \
    [file join $root_dir rtl phase_unwrap.v] \
    [file join $root_dir rtl ddc_phase_unwrap_core.v] \
    [file join $root_dir sim tb_ddc_phase_unwrap_core.v]]

puts [exec -- $xelab -nolog tb_ddc_phase_unwrap_core -s tb_ddc_phase_unwrap_core_sim]
puts [exec -- $xsim -nolog tb_ddc_phase_unwrap_core_sim -runall]

puts "Lesson 33 XSim finished"
