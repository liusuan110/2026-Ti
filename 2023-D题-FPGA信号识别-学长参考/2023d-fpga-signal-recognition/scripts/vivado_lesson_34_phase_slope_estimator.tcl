set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs lesson_34]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Lesson 34: compile phase slope estimator with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl phase_slope_estimator.v] \
    [file join $root_dir sim tb_phase_slope_estimator.v]]

puts [exec -- $xelab -nolog tb_phase_slope_estimator -s tb_phase_slope_estimator_sim]
puts [exec -- $xsim -nolog tb_phase_slope_estimator_sim -runall]

puts "Lesson 34 XSim finished"
