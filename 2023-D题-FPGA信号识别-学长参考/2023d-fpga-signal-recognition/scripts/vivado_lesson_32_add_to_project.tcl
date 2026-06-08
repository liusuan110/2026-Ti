set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ..]]
set project_path [file join $root_dir project_1.xpr]

open_project $project_path

set rtl_file [file normalize [file join $root_dir rtl phase_unwrap.v]]
set tb_file [file normalize [file join $root_dir sim tb_phase_unwrap.v]]

if {[llength [get_files -quiet $rtl_file]] == 0} {
    add_files -norecurse -fileset sources_1 $rtl_file
}

if {[llength [get_files -quiet $tb_file]] == 0} {
    add_files -norecurse -fileset sim_1 $tb_file
}

set_property top phase_unwrap [get_filesets sources_1]
set_property top tb_phase_unwrap [get_filesets sim_1]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1

close_project

puts "Lesson 32 files were added to project_1.xpr"
