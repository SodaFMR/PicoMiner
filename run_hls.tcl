# =============================================================================
# Pico Miner -- Vivado HLS Project Setup Script
#
# Usage:
#   1. Open Vivado HLS 2019.1
#   2. In the Tcl Console, run:
#        source <path_to_this_file>/run_hls.tcl
#
#   Or from command line:
#        vivado_hls -f run_hls.tcl
#
# This script creates the HLS project, adds source files, runs C simulation,
# C synthesis, and C/RTL co-simulation, then exports the IP.
#
# Target FPGA: xc7z020clg484-1 (Zynq-7020, e.g. ZedBoard)
# Clock: 10 ns (100 MHz)
# =============================================================================

# --- Project Configuration ---
set PROJECT_NAME  "pico_miner_hls"
set TOP_FUNCTION  "pico_miner"
set FPGA_PART     "xc7z020clg484-1"
set CLOCK_PERIOD  10

# --- Create Project ---
open_project ${PROJECT_NAME}
set_top ${TOP_FUNCTION}

# --- Add Source Files ---
add_files       src/pico_miner.cpp
add_files       src/pico_miner.h
add_files -tb   src/pico_miner_tb.cpp

# =============================================================================
# Solution 1: Baseline (no optimization directives)
# =============================================================================
open_solution "1_baseline"
set_part ${FPGA_PART}
create_clock -period ${CLOCK_PERIOD} -name default

# Run C Simulation
csim_design

# Run C Synthesis
csynth_design

# Run C/RTL Co-Simulation
cosim_design

# Export IP as AXI-Lite peripheral
export_design -format ip_catalog -description "Pico Miner PoW Accelerator (Baseline)" -vendor "pico_miner" -display_name "Pico Miner v1.0 Baseline"

close_solution

# =============================================================================
# Solution 2: Loop Pipelining
# The PIPELINE pragma is already in the source code (pico_miner.cpp).
# In this solution, we rely on the pragma in the source. If you want to
# experiment WITHOUT the pragma, remove it from pico_miner.cpp and add
# the directive here instead:
#   set_directive_pipeline "pico_miner/mining_loop"
# =============================================================================
open_solution "2_loop_pipelining"
set_part ${FPGA_PART}
create_clock -period ${CLOCK_PERIOD} -name default

# The PIPELINE II=1 pragma is in the source code on the mining_loop
# The ARRAY_PARTITION pragma is also in the source code

csynth_design
cosim_design

export_design -format ip_catalog -description "Pico Miner PoW Accelerator (Pipelined)" -vendor "pico_miner" -display_name "Pico Miner v1.0 Pipelined"

close_solution

# =============================================================================
# Solution 3: Directive-based optimization (alternative approach)
# Remove pragmas from source code and use directives here instead.
# This shows the teacher the "directive-based" workflow.
# =============================================================================
# open_solution "3_directives_only"
# set_part ${FPGA_PART}
# create_clock -period ${CLOCK_PERIOD} -name default
#
# # Apply directives via TCL instead of source pragmas
# set_directive_pipeline "pico_miner/mining_loop" -II 1
# set_directive_array_partition "pico_miner" -variable block_header -type complete -dim 1
#
# csynth_design
# cosim_design
# export_design -format ip_catalog
# close_solution

# =============================================================================
# Done
# =============================================================================
puts "============================================================"
puts " Pico Miner HLS -- All solutions completed successfully!"
puts "============================================================"
puts ""
puts " Next steps:"
puts "   1. Open Vivado and create a new Block Design"
puts "   2. Add the Zynq PS IP and the Pico Miner IP"
puts "   3. Run Connection Automation"
puts "   4. Generate Bitstream"
puts "   5. Export Hardware (include bitstream)"
puts "   6. Launch Xilinx SDK"
puts "   7. Create a new application project"
puts "   8. Replace helloworld.c with src/pico_miner_arm.c"
puts "   9. Build, program FPGA, and run!"
puts ""

exit
