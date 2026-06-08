set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs feature_coarse_classifier]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Compile feature_coarse_classifier with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl feature_coarse_classifier.v] \
    [file join $root_dir sim tb_feature_coarse_classifier.v]]

puts [exec -- $xelab -nolog tb_feature_coarse_classifier -s tb_feature_coarse_classifier_sim]
puts [exec -- $xsim -nolog tb_feature_coarse_classifier_sim -runall]

puts "feature_coarse_classifier XSim finished"
