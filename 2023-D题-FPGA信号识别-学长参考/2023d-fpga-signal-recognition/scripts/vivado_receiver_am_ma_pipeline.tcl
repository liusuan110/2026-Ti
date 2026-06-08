set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs receiver_am_ma]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Compile receiver_am_ma_pipeline with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl ddc_boxcar_decimator.v] \
    [file join $root_dir rtl iq_feature_extractor.v] \
    [file join $root_dir rtl feature_block_stats.v] \
    [file join $root_dir rtl am_ma_estimator_lut.v] \
    [file join $root_dir rtl receiver_am_ma_pipeline.v] \
    [file join $root_dir sim tb_receiver_am_ma_pipeline.v]]

puts [exec -- $xelab -nolog tb_receiver_am_ma_pipeline -s tb_receiver_am_ma_pipeline_sim]
puts [exec -- $xsim -nolog tb_receiver_am_ma_pipeline_sim -runall]

puts "receiver_am_ma_pipeline XSim finished"
