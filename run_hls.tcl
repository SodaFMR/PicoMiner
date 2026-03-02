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
# Architecture:
#   Bitcoin double-SHA-256 mining with midstate optimization.
#   ARM computes midstate (SHA-256 state after chunk 1 of the 80-byte header).
#   FPGA iterates nonces over chunk 2 + second full SHA-256 (128 rounds/nonce).
#   Verified against 3 real Bitcoin blocks:
#     Block 1   (first after genesis,   nonce BE 0x01e36299)
#     Block 170 (Satoshi -> Hal Finney, nonce BE 0x709e3e28)
#     Block 181 (early 2-tx block,      nonce BE 0x192d3f2f)
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
# Solution 1: Bitcoin SHA-256 Miner (baseline)
#
# The SHA-256 compress function has:
#   - 64-entry W array fully partitioned (ARRAY_PARTITION complete)
#   - W expansion (16->64) fully unrolled
#   - 64 compression rounds with PIPELINE II=1
#   - INLINE off to keep it as a separate module
#
# The mining loop calls sha256_compress twice per nonce (128 rounds total).
# No source-level pipeline pragma on the mining loop itself -- each iteration
# takes ~128 cycles (2 x 64 pipelined compress rounds).
#
# Estimated throughput: ~780 KH/s at 100 MHz (100M / 128 = 781K nonces/s).
# =============================================================================
open_solution "1_sha256_baseline"
set_part ${FPGA_PART}
create_clock -period ${CLOCK_PERIOD} -name default

# Run C Simulation
csim_design

# Run C Synthesis
csynth_design

# Run C/RTL Co-Simulation (testbench searches +/-16 nonces, should be fast)
cosim_design

# Export IP as AXI-Lite peripheral
export_design -format ip_catalog \
    -description "Pico Miner Bitcoin SHA-256 Accelerator (Midstate, II=1 compress)" \
    -vendor "pico_miner" \
    -display_name "Pico Miner v2.0 SHA-256"

close_solution

# =============================================================================
# Solution 2: SHA-256 with relaxed compress pipeline (II=2)
#
# If Solution 1 fails timing at 100 MHz (the compress round has ~5 additions
# + rotations in a single cycle), this solution relaxes the compress loop
# to II=2, halving throughput but giving more slack for routing.
#
# Estimated throughput: ~390 KH/s at 100 MHz.
# =============================================================================
open_solution "2_sha256_relaxed"
set_part ${FPGA_PART}
create_clock -period ${CLOCK_PERIOD} -name default

# Override source pragma: relax compress loop to II=2
set_directive_pipeline -II 2 "sha256_compress/compress"

csynth_design
cosim_design

export_design -format ip_catalog \
    -description "Pico Miner Bitcoin SHA-256 Accelerator (Midstate, II=2 compress)" \
    -vendor "pico_miner" \
    -display_name "Pico Miner v2.0 SHA-256 Relaxed"

close_solution

# =============================================================================
# Done
# =============================================================================
puts "============================================================"
puts " Pico Miner HLS -- All solutions completed successfully!"
puts "============================================================"
puts ""
puts " Architecture: Bitcoin double-SHA-256 with midstate optimization"
puts " Test vectors: Block 1 (0x01e36299), Block 170 (0x709e3e28), Block 181 (0x192d3f2f)"
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
puts "   8. Create Empty Application project"
puts "   9. Add src/pico_miner_arm.c to the application project"
puts "  10. Build, program FPGA, and run!"
puts ""
puts " AXI-Lite register interface (auto-generated driver functions):"
puts "   Inputs:  Set_midstate_0..7, Set_chunk2_tail_0..2,"
puts "            Set_nonce_start, Set_nonce_end, Set_target_hi"
puts "   Outputs: Get_found_nonce, Get_status"
puts ""

exit
