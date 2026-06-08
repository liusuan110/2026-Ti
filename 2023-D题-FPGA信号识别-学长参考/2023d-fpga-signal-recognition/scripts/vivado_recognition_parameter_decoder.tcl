set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs recognition_parameter_decoder]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Compile recognition_parameter_decoder with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl recognition_parameter_decoder.v] \
    [file join $root_dir sim tb_recognition_parameter_decoder.v]]

puts [exec -- $xelab -nolog tb_recognition_parameter_decoder -s tb_recognition_parameter_decoder_sim]
puts [exec -- $xsim -nolog tb_recognition_parameter_decoder_sim -runall]

puts "recognition_parameter_decoder XSim finished"
