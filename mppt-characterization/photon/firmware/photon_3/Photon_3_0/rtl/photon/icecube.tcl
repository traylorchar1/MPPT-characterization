#!usr/bin/tclsh8.4
# Sample flow script for iCEcube2
#############################################
#
User Configurable section
#############################################
set device iCE40LP8K-CM225
set top_module top_design
set proj_dir [pwd]
set output_dir "rev_1"
set edif_file "top_design"
set tool_options ":edifparser -y top_design.pcf"
###########################################
#
Tool Interface
############################################
set sbt_root $::env(SBT_DIR)
append sbt_tcl $sbt_root "/tcl/sbt_backend_synpl.tcl"
source $sbt_tcl
run_sbt_backend_auto $device $top_module
$output_dir $tool_options $edif_file
$proj_dir
exit
