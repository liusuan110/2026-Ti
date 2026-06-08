set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
set project_path [file join $root_dir project_1.xpr]

open_project $project_path

set rtl_files [list \
    [file normalize [file join $root_dir rtl dds_nco.v]] \
    [file normalize [file join $root_dir rtl ddc_mixer.v]] \
    [file normalize [file join $root_dir rtl moving_average.v]] \
    [file normalize [file join $root_dir rtl ddc_core.v]] \
    [file normalize [file join $root_dir rtl cordic_atan2_pipeline.v]] \
    [file normalize [file join $root_dir rtl ddc_phase_core_pipeline.v]] \
    [file normalize [file join $root_dir rtl phase_unwrap.v]] \
    [file normalize [file join $root_dir rtl ddc_phase_unwrap_core.v]] \
]

set tb_file [file normalize [file join $root_dir sim tb_ddc_phase_unwrap_core.v]]

foreach f $rtl_files {
    if {[llength [get_files -quiet $f]] == 0} {
        add_files -norecurse -fileset sources_1 $f
    }
}

if {[llength [get_files -quiet $tb_file]] == 0} {
    add_files -norecurse -fileset sim_1 $tb_file
}

set_property top ddc_phase_unwrap_core [get_filesets sources_1]
set_property top tb_ddc_phase_unwrap_core [get_filesets sim_1]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

close_project

puts "Lesson 33 files were added to project_1.xpr"
