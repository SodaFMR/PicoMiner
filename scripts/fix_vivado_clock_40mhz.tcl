# =============================================================================
# Pico Miner -- Vivado Clock/Bitstream Fix Script (40 MHz)
#
# Purpose:
#   Force the Zynq PS FCLK_CLK0 to 40 MHz (25 ns), regenerate the BD wrapper,
#   reset synthesis/implementation, and rebuild bitstream.
#
# Usage (GUI, project already open):
#   source scripts/fix_vivado_clock_40mhz.tcl
#
# Usage (batch):
#   vivado -mode batch -source scripts/fix_vivado_clock_40mhz.tcl -tclargs <path/to/project.xpr> [bd_name] [ps_cell] [clk_mhz]
#
# Defaults:
#   bd_name = miner
#   ps_cell = processing_system7_0
#   clk_mhz = 40.0
# =============================================================================

proc pm_fail {msg} {
    puts "ERROR: $msg"
    return -code error $msg
}

set pm_xpr_path ""
set pm_bd_name "miner"
set pm_ps_cell "processing_system7_0"
set pm_clk_mhz 40.0

if {[info exists argc] && $argc >= 1} {
    set pm_xpr_path [lindex $argv 0]
}
if {[info exists argc] && $argc >= 2} {
    set pm_bd_name [lindex $argv 1]
}
if {[info exists argc] && $argc >= 3} {
    set pm_ps_cell [lindex $argv 2]
}
if {[info exists argc] && $argc >= 4} {
    set pm_clk_mhz [lindex $argv 3]
}

if {$pm_xpr_path ne ""} {
    puts "Opening project: $pm_xpr_path"
    open_project $pm_xpr_path
}

if {[catch {current_project} pm_project] || $pm_project eq ""} {
    pm_fail "No open Vivado project. Open the project first or pass an .xpr path."
}

puts "Refreshing IP catalog..."
update_ip_catalog -rebuild

set pm_bd_files [get_files -quiet -all */${pm_bd_name}.bd]
if {[llength $pm_bd_files] == 0} {
    set pm_bd_files [get_files -quiet -all ${pm_bd_name}.bd]
}
if {[llength $pm_bd_files] == 0} {
    pm_fail "Could not find Block Design '${pm_bd_name}.bd' in project."
}

set pm_bd_file [lindex $pm_bd_files 0]
puts "Using Block Design file: $pm_bd_file"

if {[catch {open_bd_design $pm_bd_file} pm_open_err]} {
    puts "Info: open_bd_design returned: $pm_open_err"
}

if {[catch {current_bd_design $pm_bd_name} pm_curr_bd_err]} {
    puts "Info: current_bd_design '${pm_bd_name}' returned: $pm_curr_bd_err"
}

set pm_ps [get_bd_cells -quiet $pm_ps_cell]
if {[llength $pm_ps] == 0} {
    pm_fail "Could not find PS cell '${pm_ps_cell}' in BD '${pm_bd_name}'."
}

set pm_old_clk [get_property CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ $pm_ps]
puts "Old PS FCLK_CLK0 frequency: ${pm_old_clk} MHz"

set_property -dict [list CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ $pm_clk_mhz] $pm_ps

validate_bd_design
save_bd_design

set pm_new_clk [get_property CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ $pm_ps]
puts "New PS FCLK_CLK0 frequency: ${pm_new_clk} MHz"

puts "Regenerating BD output products and wrapper..."
generate_target all [get_files $pm_bd_file]
make_wrapper -files [get_files $pm_bd_file] -top

set pm_ips [get_ips -quiet]
if {[llength $pm_ips] > 0} {
    puts "Upgrading IPs to latest available revisions..."
    upgrade_ip -quiet $pm_ips
}

set pm_wrapper_guess [file join [file dirname $pm_bd_file] hdl ${pm_bd_name}_wrapper.v]
if {[file exists $pm_wrapper_guess]} {
    if {[llength [get_files -quiet $pm_wrapper_guess]] == 0} {
        add_files -norecurse $pm_wrapper_guess
    }
}
update_compile_order -fileset sources_1

if {[llength [get_runs -quiet synth_1]] == 0 || [llength [get_runs -quiet impl_1]] == 0} {
    pm_fail "Project runs synth_1/impl_1 were not found."
}

puts "Resetting runs..."
reset_run synth_1
reset_run impl_1

puts "Launching implementation to bitstream..."
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

open_run impl_1
report_timing_summary -max_paths 20 -report_unconstrained -file timing_summary_postroute_40mhz.rpt

set pm_clk_obj [get_clocks -quiet clk_fpga_0]
if {[llength $pm_clk_obj] > 0} {
    puts "Clock 'clk_fpga_0' period now: [get_property PERIOD [lindex $pm_clk_obj 0]] ns"
} else {
    puts "Info: clock object 'clk_fpga_0' was not found by name."
}

set pm_impl_status [get_property STATUS [get_runs impl_1]]
puts "impl_1 status: $pm_impl_status"

puts "Done."
