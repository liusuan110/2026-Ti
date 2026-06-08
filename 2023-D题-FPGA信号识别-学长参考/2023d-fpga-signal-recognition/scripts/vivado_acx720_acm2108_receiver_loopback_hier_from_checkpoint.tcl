set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
set out_dir [file join $root_dir outputs acx720_acm2108_receiver_loopback_synth]

open_checkpoint [file join $out_dir post_synth.dcp]
report_utilization -hierarchical -file [file join $out_dir utilization_hier_synth.rpt]

puts "ACX720 ACM2108 receiver loopback hierarchical utilization report finished"
