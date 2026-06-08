set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs lesson_30]

puts "Lesson 30: compile pipeline CORDIC with Vivado XSim"
puts "Project root: $root_dir"

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts [exec -- $xvlog -nolog [file join $root_dir rtl cordic_atan2_pipeline.v] [file join $root_dir sim tb_cordic_atan2_pipeline.v]]
puts [exec -- $xelab -nolog tb_cordic_atan2_pipeline -s tb_cordic_atan2_pipeline_sim]
puts [exec -- $xsim -nolog tb_cordic_atan2_pipeline_sim -runall]

puts "Lesson 30 XSim finished"
