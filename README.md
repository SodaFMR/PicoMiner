# Pico Miner

**FPGA-Based Bitcoin SHA-256 Mining Accelerator**

A proof-of-concept project that demonstrates real Bitcoin mining (double-SHA-256 with midstate optimization) accelerated on FPGA hardware using Xilinx Vivado HLS. Designed for the Zynq-7020 SoC (ZedBoard).

---

## Overview

Cryptocurrency mining relies on a brute-force search for a **nonce** value that, when included in the block header and double-hashed with SHA-256, produces a hash below a **difficulty target**. This is the core Proof of Work (PoW) mechanism used in Bitcoin.

**Pico Miner** implements real Bitcoin double-SHA-256 mining on FPGA hardware:

- **Real SHA-256**: Implements the full FIPS 180-4 SHA-256 compression function, not a toy hash
- **Midstate optimization**: The ARM processor precomputes the SHA-256 state after the first 64-byte chunk of the 80-byte block header; the FPGA only processes the remaining 16 bytes (chunk 2) plus the second full SHA-256 hash -- 128 compression rounds per nonce
- **Real test vector**: Verified against **Bitcoin Block 170** (Satoshi -> Hal Finney, the first non-coinbase transaction)
- **AXI-Lite interface**: ARM controls the FPGA miner via memory-mapped registers
- **HLS optimization**: Pipeline and array partition directives demonstrate hardware acceleration techniques

> For a detailed explanation of the design decisions and architecture, see the [Proof of Concept document](doc/poc.tex).

## Project Structure

```
PicoMiner/
├── src/                               # Project implementation
│   ├── pico_miner.h                   #   Header: SHA-256 constants, interface definition
│   ├── pico_miner.cpp                 #   HLS source: SHA-256 compress + mining loop
│   ├── pico_miner_tb.cpp              #   HLS testbench: NIST vector + Block 170 mining
│   └── pico_miner_arm.c               #   ARM driver: midstate computation + HW control
├── doc/
│   └── poc.tex                        # Proof of Concept document (LaTeX)
├── examples/                          # Course material provided by the teacher
│   ├── 1. sws2leds_withAXIGPIO/       #   Basic Zynq: read switches, write LEDs via AXI GPIO
│   ├── 3.7 hls2vhdl_complex_sqr_float/#   HLS: complex number squaring with float I/O
│   ├── 5.hls_directives_performance/  #   HLS: vector sum/dot-product with pipeline/unroll/partition
│   ├── complex_prod_sources/          #   HLS: complex number product with float arrays
│   ├── convo5x/                       #   HLS: 5x5 image convolution with line buffers
│   ├── hls2vhdl_array2saxilite/       #   HLS: array reorder via AXI-Lite (full HLS-to-board flow)
│   ├── hls2vhdl_mycnt/                #   HLS: counter with static state (full HLS-to-board flow)
│   ├── labdocs/                       #   Lab instruction PDFs (Labs 1-4)
│   ├── labsource/                     #   Lab source code (matrix multiply, YUV filter, DCT, FIR)
│   ├── ug871-design-files2016.2/      #   Xilinx UG871 tutorial design files (HLS examples)
│   └── zynq2mblaze_sws_leds/         #   Zynq ARM + MicroBlaze communication via shared BRAM
├── run_hls.tcl                        # Vivado HLS automation script
└── README.md                          # This file
```

## How It Works

### Bitcoin Double-SHA-256 Mining

1. **Block header**: 80 bytes containing version, previous block hash, merkle root, timestamp, difficulty bits, and nonce
2. **Double hash**: `hash = SHA-256(SHA-256(header))` -- the 80-byte header is hashed twice
3. **Difficulty check**: The resulting 256-bit hash must be below the target (leading zeros)
4. **Brute force**: Increment the 32-bit nonce and repeat until a valid hash is found

### Midstate Optimization

The 80-byte header spans two SHA-256 blocks (64 bytes each, with padding):

| Chunk | Bytes | Contents | Processed by |
|-------|-------|----------|-------------|
| Chunk 1 | 0-63 | version + prev_hash + merkle[0..27] | ARM (once) |
| Chunk 2 | 64-79 + padding | merkle[28..31] + timestamp + bits + **nonce** | FPGA (per nonce) |

Since only the nonce changes between attempts, chunk 1 produces a fixed intermediate SHA-256 state called the **midstate**. The ARM computes this once, and the FPGA starts from the midstate for each nonce -- cutting the work in half.

### Per-Nonce FPGA Work

For each nonce candidate, the FPGA performs:
1. **First SHA-256 (chunk 2)**: 64 compression rounds starting from the midstate
2. **Second SHA-256**: 64 compression rounds on the 32-byte first hash (with SHA-256 initial values)
3. **Difficulty check**: Compare `final_hash[0]` (most significant 32 bits) against `target_hi`

Total: **128 SHA-256 compression rounds per nonce**.

### System Architecture

```
┌─────────────────────────┐      AXI-Lite      ┌─────────────────────────┐
│   ARM Cortex-A9 (PS)    │◄──────────────────►│   Pico Miner IP (PL)    │
│                         │                    │                         │
│   pico_miner_arm.c      │  Write:            │   pico_miner.cpp        │
│   - Parse block header  │  midstate[8]       │   - sha256_compress()   │
│   - Compute midstate    │  chunk2_tail[3]    │     64 rounds, II=1     │
│   - Configure & start   │  nonce_start/end   │   - Mining loop:        │
│   - Poll & read results │  target_hi         │     2x compress/nonce   │
│   - SW verification     │                    │   - Difficulty check    │
│                         │  Read:             │                         │
│                         │  found_nonce       │                         │
│                         │  status            │                         │
└─────────────────────────┘                    └─────────────────────────┘
```

### Test Vector: Bitcoin Block 170

The project uses **Block 170** as its test vector -- the first block containing a non-coinbase transaction (Satoshi Nakamoto sent 10 BTC to Hal Finney on January 12, 2009).

| Field | Value |
|-------|-------|
| Nonce (LE) | `0x283e9e70` |
| Nonce (BE) | `0x709e3e28` |
| Block hash | `00000000d1145790a8694403d4063f323d499e655c83426834d4ce2f8dd4a2ee` |
| Target (hi word) | `0x00000000` (32 leading zero bits) |

## Prerequisites

- **Vivado HLS 2019.1** (part of Vivado Design Suite, not Vivado Lab)
- **Vivado 2019.1** (for block design and bitstream generation)
- **Xilinx SDK 2019.1** (for ARM software development)
- **Target FPGA**: Zynq-7020 (`xc7z020clg484-1`) -- e.g., ZedBoard

> **Note**: Vivado Lab is only the FPGA programming tool. You need the full Vivado Design Suite which includes Vivado HLS for C-to-RTL synthesis.

## Quick Start

### Step 1: Vivado HLS -- Synthesize the Mining IP

```bash
# Option A: Run the automated TCL script
vivado_hls -f run_hls.tcl

# Option B: Manual (Vivado HLS GUI)
# 1. Create new project, set top function to "pico_miner"
# 2. Add src/pico_miner.cpp and src/pico_miner.h as source files
# 3. Add src/pico_miner_tb.cpp as testbench
# 4. Set target part: xc7z020clg484-1, clock: 10ns (100 MHz)
# 5. Run C Simulation -> should print "ALL TESTS PASSED"
# 6. Run C Synthesis -> generates RTL + performance report
# 7. Run C/RTL Co-Simulation -> verifies RTL matches C behavior
# 8. Export RTL (IP Catalog format)
```

The `run_hls.tcl` script builds two solutions:
- **Solution 1** (`1_sha256_baseline`): SHA-256 compress with II=1 pipeline (~780 KH/s)
- **Solution 2** (`2_sha256_relaxed`): SHA-256 compress with II=2 pipeline (~390 KH/s, more timing margin)

### Step 2: Vivado -- Create Block Design

1. Open Vivado 2019.1, create a new project targeting `xc7z020clg484-1`
2. Create a Block Design
3. Add the **ZYNQ7 Processing System** IP
4. Add the **Pico Miner** IP (from the HLS export, add the IP repository)
5. In `processing_system7_0`, keep `FCLK_CLK0 = 100 MHz` (default)
6. Run **Connection Automation** (connects AXI-Lite automatically)
7. Generate bitstream:

```tcl
open_bd_design [get_files miner.bd]
set_property -dict [list CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100.0}] [get_bd_cells processing_system7_0]
validate_bd_design
save_bd_design
generate_target all [get_files miner.bd]
make_wrapper -files [get_files miner.bd] -top
update_compile_order -fileset sources_1
reset_run synth_1
reset_run impl_1
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
```

8. **Export Hardware** (File > Export > Export Hardware, include bitstream)
9. **Launch SDK** (File > Launch SDK)

> **Important**: The new SHA-256 IP has different ports than the old PicoHash version (midstate[8], chunk2_tail[3] instead of block_header[4]). The block design must be recreated from scratch.

### Step 3: Xilinx SDK -- ARM Software

1. Create a new **Application Project** (Empty Application template)
2. Add `src/pico_miner_arm.c` to the `src/` folder
3. Build the project
4. Program the FPGA and run the application
5. Observe results on the UART serial terminal (115200 baud)

> **Note**: Use "Empty Application" template, not "Hello World". The SDK Empty Application template does not include `platform.h` -- the driver uses `xil_cache.h` directly.

## Expected Output

### C Simulation (Vivado HLS)

```
################################################################
#                                                              #
#         PICO MINER -- Bitcoin SHA-256 HLS Testbench          #
#       FPGA-Based Proof of Work Mining Accelerator            #
#                                                              #
################################################################

================================================================
TEST 1: SHA-256 Known Vector -- SHA256("abc")
================================================================
  Computed: ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
  Expected: ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
  SHA-256 test: [OK]

================================================================
TEST 2: Bitcoin Block 170 -- Real Mining Test
================================================================
  Midstate: ...
  [HW] Status: FOUND
  [HW] Found nonce: 0x709e3e28
  [HW] Nonce matches Bitcoin Block 170: [OK]

================================================================
TEST 3: No Solution in Range
================================================================
  No-solution test: [OK]

================================================================
ALL TESTS PASSED -- Bitcoin double-SHA-256 mining verified.
================================================================
```

### ARM Driver Output (UART)

```
############################################################
#                                                          #
#      PICO MINER -- Bitcoin SHA-256 FPGA Accelerator      #
#         ARM Driver (Zynq PS) + HLS IP (PL)               #
#                                                          #
############################################################

[INFO] Test vector: Bitcoin Block 170
[INFO] Known nonce (BE): 0x709E3E28
[MINING] Starting hardware miner...
[MINING] Hardware miner finished!
[HW RESULT] Status: FOUND
[HW RESULT] Winning nonce (BE): 0x709E3E28
  >>> ALL RESULTS MATCH -- VERIFICATION PASSED <<<
```

## HLS Optimization Strategy

### SHA-256 Compress Function

The core `sha256_compress()` function implements the full 64-round SHA-256 compression:

- **W[64] message schedule**: Fully partitioned (`ARRAY_PARTITION complete`) for parallel access
- **W expansion (16->64)**: Fully unrolled
- **64 compression rounds**: Pipelined with `PIPELINE II=1`
- **INLINE off**: Kept as separate module (called twice per nonce)

### Performance Estimates

| Solution | Compress II | Rounds/nonce | Throughput (est.) |
|----------|------------|-------------|-------------------|
| Baseline (II=1) | 1 | 128 | ~780 KH/s |
| Relaxed (II=2) | 2 | 256 | ~390 KH/s |

At 100 MHz, with II=1 compress: 100M cycles / 128 rounds = ~781K nonces/second.

> **Note**: These are estimates. Actual performance depends on HLS synthesis results and timing closure. The SHA-256 version has NOT yet been synthesized -- timing at 100 MHz is not yet verified.

## AXI-Lite Register Interface

Vivado HLS 2019.1 maps array arguments as individual scalar registers:

| Direction | Register | SDK Driver Function |
|-----------|----------|-------------------|
| Input | midstate[0..7] | `XPico_miner_Set_midstate_0` .. `_7` |
| Input | chunk2_tail[0..2] | `XPico_miner_Set_chunk2_tail_0` .. `_2` |
| Input | nonce_start | `XPico_miner_Set_nonce_start` |
| Input | nonce_end | `XPico_miner_Set_nonce_end` |
| Input | target_hi | `XPico_miner_Set_target_hi` |
| Output | found_nonce | `XPico_miner_Get_found_nonce` |
| Output | status | `XPico_miner_Get_status` |

## File Descriptions

| File | Purpose |
|---|---|
| `src/pico_miner.h` | Header: SHA-256 constants (H0-H7, K[64]), function prototype, interface defines |
| `src/pico_miner.cpp` | HLS source: `sha256_compress()` + double-SHA-256 mining loop with midstate optimization |
| `src/pico_miner_tb.cpp` | HLS testbench: NIST SHA256("abc") vector + Bitcoin Block 170 mining + no-solution test |
| `src/pico_miner_arm.c` | ARM driver: Block 170 header parsing, midstate computation, HW mining, SW verification |
| `doc/poc.tex` | LaTeX proof of concept document with SHA-256 algorithm and architecture description |
| `run_hls.tcl` | TCL script: builds 2 solutions (II=1 baseline, II=2 relaxed), runs csim/csynth/cosim/export |

## License

This project is for educational purposes.
