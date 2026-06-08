set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs tone_score_accumulator]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Compile tone_score_accumulator with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl tone_score_accumulator.v] \
    [file join $root_dir sim tb_tone_score_accumulator.v]]

puts [exec -- $xelab -nolog tb_tone_score_accumulator -s tb_tone_score_accumulator_sim]
puts [exec -- $xsim -nolog tb_tone_score_accumulator_sim -runall]

puts "tone_score_accumulator XSim finished"
