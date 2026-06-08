set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
cd $root_dir

set synth_dir [file join $root_dir outputs acx720_acm2108_receiver_loopback_synth]
set out_dir [file join $root_dir outputs acx720_acm2108_receiver_loopback_impl]
file mkdir $out_dir

open_checkpoint [file join $synth_dir post_synth.dcp]

opt_design
place_design
phys_opt_design
route_design

report_utilization -file [file join $out_dir utilization_impl.rpt]
report_timing_summary -file [file join $out_dir timing_impl.rpt]
write_checkpoint -force [file join $out_dir post_route.dcp]
write_bitstream -force [file join $out_dir acx720_acm2108_receiver_loopback_top.bit]

puts "ACX720 ACM2108 receiver loopback implementation and bitstream finished"
