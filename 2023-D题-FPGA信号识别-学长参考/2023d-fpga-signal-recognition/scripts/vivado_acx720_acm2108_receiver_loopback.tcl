set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

file mkdir [file join $root_dir outputs acx720_acm2108_receiver_loopback]

set vivado_exe [file normalize [info nameofexecutable]]
set vivado_bin [file dirname $vivado_exe]
if {![file exists [file join $vivado_bin xvlog.bat]]} {
    set vivado_bin [file normalize [file join $vivado_bin .. ..]]
}
set xvlog [file join $vivado_bin xvlog.bat]
set xelab [file join $vivado_bin xelab.bat]
set xsim [file join $vivado_bin xsim.bat]

puts "Compile ACX720 ACM2108 receiver loopback top with Vivado XSim"
puts "Project root: $root_dir"

puts [exec -- $xvlog -nolog \
    [file join $root_dir rtl button_press_pulse.v] \
    [file join $root_dir rtl dac_2023d_signal_source.v] \
    [file join $root_dir rtl ddc_boxcar_decimator.v] \
    [file join $root_dir rtl iq_feature_extractor.v] \
    [file join $root_dir rtl iq_mag_sq_extractor.v] \
    [file join $root_dir rtl iq_dot_sign_extractor.v] \
    [file join $root_dir rtl iq_dot_cross_extractor.v] \
    [file join $root_dir rtl unsigned_block_minmax.v] \
    [file join $root_dir rtl feature_block_stats.v] \
    [file join $root_dir rtl feature_coarse_classifier.v] \
    [file join $root_dir rtl am_ma_estimator_lut.v] \
    [file join $root_dir rtl receiver_am_ma_pipeline.v] \
    [file join $root_dir rtl cordic_atan2_pipeline.v] \
    [file join $root_dir rtl fm_delta_phase_stats.v] \
    [file join $root_dir rtl fm_delta_to_hz.v] \
    [file join $root_dir rtl fm_mf_from_delta.v] \
    [file join $root_dir rtl receiver_fm_delta_pipeline.v] \
    [file join $root_dir rtl tone_score_accumulator.v] \
    [file join $root_dir rtl tone_score_bank5.v] \
    [file join $root_dir rtl iq_feature_tone_score_pipeline.v] \
    [file join $root_dir rtl receiver_coarse_pipeline.v] \
    [file join $root_dir rtl receiver_tone_score_pipeline.v] \
    [file join $root_dir rtl receiver_recognition_pipeline.v] \
    [file join $root_dir rtl recognition_parameter_decoder.v] \
    [file join $root_dir rtl receiver_recognition_with_params.v] \
    [file join $root_dir rtl signed_block_minmax.v] \
    [file join $root_dir rtl transition_rate_estimator.v] \
    [file join $root_dir rtl fsk_h_from_sep.v] \
    [file join $root_dir rtl receiver_ask_rate_pipeline.v] \
    [file join $root_dir rtl receiver_psk_rate_pipeline.v] \
    [file join $root_dir rtl receiver_fsk_rate_h_pipeline.v] \
    [file join $root_dir rtl receiver_keying_recognition_pipeline.v] \
    [file join $root_dir rtl receiver_full_scope_pipeline.v] \
    [file join $root_dir rtl receiver_nco_frontend.v] \
    [file join $root_dir rtl receiver_stream_core.v] \
    [file join $root_dir rtl acx720_acm2108_receiver_loopback_top.v] \
    [file join $root_dir sim tb_acx720_acm2108_receiver_loopback_top.v]]

puts [exec -- $xelab -nolog tb_acx720_acm2108_receiver_loopback_top -s tb_acx720_loopback_sim]
puts [exec -- $xsim -nolog tb_acx720_loopback_sim -runall]

puts "ACX720 ACM2108 receiver loopback XSim finished"
