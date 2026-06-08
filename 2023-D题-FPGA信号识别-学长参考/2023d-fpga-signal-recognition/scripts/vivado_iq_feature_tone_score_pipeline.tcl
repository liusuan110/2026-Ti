set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs iq_feature_tone_score]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Compile iq_feature_tone_score_pipeline with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl iq_feature_extractor.v] \
    [file join $root_dir rtl tone_score_accumulator.v] \
    [file join $root_dir rtl tone_score_bank5.v] \
    [file join $root_dir rtl iq_feature_tone_score_pipeline.v] \
    [file join $root_dir sim tb_iq_feature_tone_score_pipeline.v]]

puts [exec -- $xelab -nolog tb_iq_feature_tone_score_pipeline -s tb_iq_feature_tone_score_pipeline_sim]
puts [exec -- $xsim -nolog tb_iq_feature_tone_score_pipeline_sim -runall]

puts "iq_feature_tone_score_pipeline XSim finished"
