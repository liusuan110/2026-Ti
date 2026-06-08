set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs ddc_boxcar]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Compile ddc_boxcar_decimator with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl ddc_boxcar_decimator.v] \
    [file join $root_dir sim tb_ddc_boxcar_decimator.v]]

puts [exec -- $xelab -nolog tb_ddc_boxcar_decimator -s tb_ddc_boxcar_decimator_sim]
puts [exec -- $xsim -nolog tb_ddc_boxcar_decimator_sim -runall]

puts "ddc_boxcar_decimator XSim finished"
