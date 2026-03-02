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
# Solution 1: Baseline with timing-safe pipeline (II=2)
#
# The source code embeds #pragma HLS PIPELINE II=2 inside mining_loop.
# II=2 lets HLS place a register between the two 32x32 multiplies,
# keeping every pipeline stage under 10 ns on Zynq-7020 (-1).
# Throughput: ~50 MH/s at 100 MHz.
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
export_design -format ip_catalog -description "Pico Miner PoW Accelerator (Pipelined II=2)" -vendor "pico_miner" -display_name "Pico Miner v1.0 Pipelined"

close_solution

# =============================================================================
# Solution 2: Relaxed pipeline (II=4, directive override)
#
# Overrides the source-level II=2 with a more conservative II=4 via
# Tcl directive.  Lower resource usage, still good throughput (~25 MH/s).
# Demonstrates the directive-based workflow for the teacher.
# =============================================================================
open_solution "2_relaxed_pipeline"
set_part ${FPGA_PART}
create_clock -period ${CLOCK_PERIOD} -name default

# Override source pragma with a wider II
set_directive_pipeline -II 4 "pico_miner/mining_loop"

csynth_design
cosim_design

export_design -format ip_catalog -description "Pico Miner PoW Accelerator (Pipelined II=4)" -vendor "pico_miner" -display_name "Pico Miner v1.0 Pipelined II=4"

close_solution

# =============================================================================
# Solution 3: Directive-based optimization (alternative approach)
# Uses only Tcl directives (the source #pragma PIPELINE is overridden).
# This shows the teacher the "directive-based" workflow.
# =============================================================================
# open_solution "3_directives_only"
# set_part ${FPGA_PART}
# create_clock -period ${CLOCK_PERIOD} -name default
#
# # Apply directives via TCL instead of source pragmas
# set_directive_pipeline "pico_miner/mining_loop" -II 2
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
puts " IMPORTANT:"
puts "   run_hls.tcl only builds/exports the HLS IP."
puts "   Bitstream timing depends on Vivado BD clock configuration."
puts "   In Vivado, keep processing_system7_0/FCLK_CLK0 at 100 MHz (10 ns)."
puts ""
puts " Next steps:"
puts "   1. Open Vivado and create a new Block Design"
puts "   2. Add the Zynq PS IP and the Pico Miner IP"
puts "   3. Keep processing_system7_0/FCLK_CLK0 = 100 MHz (default)"
puts "   4. Run Connection Automation"
puts "   5. Generate Bitstream"
puts "   6. Export Hardware (include bitstream)"
puts "   7. Launch Xilinx SDK"
puts "   8. Create a new application project"
puts "   9. Add src/pico_miner_arm.c to the application project"
puts "  10. Build, program FPGA, and run!"
puts ""

exit
