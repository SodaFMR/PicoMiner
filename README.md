# Pico Miner

**FPGA-Based Bitcoin SHA-256 Mining Accelerator**

Bitcoin double-SHA-256 mining with midstate optimization, implemented on a Zynq-7020 FPGA (ZedBoard) using Vivado HLS 2019.1. The demo mines Bitcoin Block 939260 (March 4, 2026) from nonce=0 in approximately 3 minutes, with real-time progress on UART terminal and ZedBoard LEDs.

---

## Overview

Cryptocurrency mining relies on a brute-force search for a **nonce** value that, when included in the block header and double-hashed with SHA-256, produces a hash below a **difficulty target**. This is the core Proof of Work (PoW) mechanism used in Bitcoin.

**Pico Miner** accelerates Bitcoin double-SHA-256 mining on FPGA hardware:

- **SHA-256 (FIPS 180-4)**: Full 64-round compression function
- **Midstate optimization**: ARM precomputes SHA-256 state after chunk 1 of the 80-byte header; FPGA processes only chunk 2 plus the second SHA-256 hash -- 128 compression rounds per nonce
- **Demo block**: Block 939260 (March 4, 2026, difficulty 144,398,401,518,101) mined from nonce=0 (~135M nonces, ~3 minutes)
- **Progress feedback**: UART terminal output with hash rate, elapsed time, and percentage; 8 ZedBoard LEDs light up proportionally to search progress
- **Full-range mining**: Brute-force search from nonce=0, identical to how Bitcoin miners operate
- **AXI-Lite interface**: ARM controls the FPGA miner via memory-mapped registers
- **HLS directives**: Pipeline II=1, array partition, and unroll directives for hardware optimization

> For a detailed explanation of the design and architecture, see the [project memory](doc/poc.tex) (bilingual ES/EN LaTeX document).

## Project Structure

```
PicoMiner/
├── src/
│   ├── pico_miner.h               # Header: SHA-256 constants, interface definition
│   ├── pico_miner.cpp             # HLS source: SHA-256 compress + mining loop
│   ├── pico_miner_tb.cpp          # HLS testbench: 5 tests (NIST, Block 1, Block 939260)
│   └── pico_miner_arm.c           # ARM driver: Block 939260 full-range demo with LEDs
├── doc/
│   └── poc.tex                    # Project memory (bilingual ES/EN, LaTeX)
├── examples/                      # Course material provided by the teacher
│   ├── 1. sws2leds_withAXIGPIO/
│   ├── 3.7 hls2vhdl_complex_sqr_float/
│   ├── 5.hls_directives_performance/
│   ├── complex_prod_sources/
│   ├── convo5x/
│   ├── hls2vhdl_array2saxilite/
│   ├── hls2vhdl_mycnt/
│   ├── labdocs/
│   ├── labsource/
│   ├── ug871-design-files2016.2/
│   └── zynq2mblaze_sws_leds/
├── run_hls.tcl                    # Vivado HLS automation script (2 solutions)
└── README.md                      # This file
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
3. **Difficulty check**: Compare `final_hash[7]` against `target_hi`

Total: **128 SHA-256 compression rounds per nonce**.

> **Note on byte order**: Bitcoin displays hashes in reversed byte order. The leading zeros in the display hash correspond to `final_hash[7]` (the last word of the SHA-256 output), not `final_hash[0]`.

### System Architecture

```
┌─────────────────────────┐      AXI-Lite      ┌─────────────────────────┐
│   ARM Cortex-A9 (PS)    │◄──────────────────►│   Pico Miner IP (PL)    │
│                         │                    │                         │
│   pico_miner_arm.c      │  Write:            │   pico_miner.cpp        │
│   - Parse block header  │  midstate[8]       │   - sha256_compress()   │
│   - Compute midstate    │  chunk2_tail[3]    │     64 rounds, II=1     │
│   - Send 1M-nonce chunks│  nonce_start/end   │   - Mining loop:        │
│   - Print progress/LEDs │  target_hi         │     2x compress/nonce   │
│   - SW verification     │                    │   - Difficulty check    │
│                         │  Read:             │                         │
│                         │  found_nonce       │                         │
│                         │  status            │                         │
└─────────────────────────┘                    └─────────────────────────┘
                │
                │  AXI GPIO
                ▼
        ┌───────────────┐
        │  8 LEDs       │  Progress bar: 0-8 LEDs proportional to search
        │  (ZedBoard)   │  All ON = block mined
        └───────────────┘
```

### Chunked Mining Strategy

The ARM driver breaks the full nonce search into chunks of 1,000,000 nonces per FPGA invocation. Between chunks, the ARM:
- Prints progress (nonces searched, percentage, elapsed time, hash rate) to UART
- Updates the LED progress bar (8 LEDs light up proportionally)
- Checks if the FPGA found a valid nonce

This provides real-time feedback during the ~3 minute mining run.

## Demo Block: Block 939260

| Field | Value |
|-------|-------|
| **Height** | 939,260 |
| **Date** | March 4, 2026 |
| **Difficulty** | 144,398,401,518,101 |
| **Nonce (LE)** | `0x1E740A08` |
| **Nonce (BE)** | `0x080A741E` (~134.9M) |
| **Block hash** | `000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06` |
| **Est. mining time** | ~3 minutes at 781 KH/s |

### Nonce Byte Order

Bitcoin serializes nonces in **little-endian** (LE) in the block header. SHA-256 processes words in **big-endian** (BE). The FPGA iterates the BE nonce word directly, searching in a different order than a LE miner but covering the same 2^32 nonce space and producing identical hashes for any given nonce value.

## Verification

### HLS Testbench (5 Tests)

| Test | Description | Method |
|------|-------------|--------|
| 1 | SHA-256("abc") NIST FIPS 180-4 vector | SW hash vs known digest |
| 2 | Block 1 full-range mining from nonce=0 | SW brute-force (~31.8M nonces) |
| 3 | Block 1 HW mining (narrow ±16 window) | HLS accelerator call |
| 4 | Block 939260 HW mining (narrow ±16 window) | HLS accelerator call |
| 5 | No-solution range | Verify "not found" status |

The narrow ±16 windows for HW tests keep C/RTL co-simulation fast (32 nonces × 128 cycles/nonce). The full-range search in Test 2 runs in pure software.

### ARM Driver Verification

After the FPGA finds a nonce, the ARM driver:
1. Runs the SW golden model (full double-SHA-256) on the found nonce
2. Displays the computed hash in Bitcoin display order
3. Compares the found nonce against the known blockchain nonce
4. Reports verification status on UART terminal

## Prerequisites

- **Vivado HLS 2019.1** (part of Vivado Design Suite)
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
# 5. Run C Simulation -> should print "ALL TESTS PASSED (5 tests)"
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
5. Add an **AXI GPIO** IP, configure for 8-bit output (LEDs)
6. In `processing_system7_0`, keep `FCLK_CLK0 = 100 MHz` (default)
7. Run **Connection Automation** (connects AXI-Lite automatically)
8. Connect GPIO output to ZedBoard LED pins (constrain in XDC)
9. Generate bitstream:

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

10. **Export Hardware** (File > Export > Export Hardware, include bitstream)
11. **Launch SDK** (File > Launch SDK)

### Step 3: Xilinx SDK -- ARM Software

1. Create a new **Application Project** (Empty Application template)
2. Add `src/pico_miner_arm.c` to the `src/` folder
3. Build the project
4. Program the FPGA and run the application
5. Observe results on the UART serial terminal (115200 baud)
6. Watch the ZedBoard LEDs light up as mining progresses

> **Note**: Use "Empty Application" template, not "Hello World". The driver uses `xil_cache.h` directly (not `platform.h`).

## Expected Output

### C Simulation (Vivado HLS)

```
################################################################
#                                                              #
#         PICO MINER -- Bitcoin SHA-256 HLS Testbench          #
#           Zynq-7020 (ZedBoard) + Vivado HLS 2019.1          #
#                                                              #
################################################################

================================================================
TEST 1: SHA-256 Known Vector -- SHA256("abc")
  Reference: NIST FIPS 180-4
================================================================
  Computed: ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
  Expected: ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
  SHA-256 NIST test: [OK]

================================================================
TEST 2: Mine Block 1 -- Full Range Search from Nonce=0
  Brute-force search for Block 1's nonce (~31.8M iterations)
================================================================
  [SW MINING] Searching from nonce_be=0x00000000...
  [SW MINING] FOUND! Nonce (BE): 0x01e36299
  [SW MINING] Matches known Block 1 nonce: [OK]

================================================================
TEST: Mine Block 1 -- HW Accelerator
================================================================
  [HW] Status: FOUND
  [HW] Found nonce (BE): 0x01e36299
  [HW] Nonce matches Block 1: [OK]

================================================================
TEST: Mine Block 939260 -- HW Accelerator
================================================================
  [HW] Status: FOUND
  [HW] Found nonce (BE): 0x080a741e
  [HW] Nonce matches Block 939260: [OK]

================================================================
TEST 5: No Solution in Range
================================================================
  Status: NOT FOUND (expected)
  No-solution test: [OK]

================================================================
ALL TESTS PASSED (5 tests)
================================================================
```

### ARM Driver Output (UART)

```
############################################################
#                                                          #
#              PICO MINER -- Bitcoin on FPGA               #
#          Double-SHA-256 with Midstate Optimization        #
#            Zynq-7020 (ZedBoard) + Vivado HLS             #
#                                                          #
############################################################

  Target block:  #939260 (March 4, 2026)
  Difficulty:    144,398,401,518,101
  Algorithm:     Bitcoin double-SHA-256 (FIPS 180-4)
  Optimization:  Midstate precomputation (ARM -> FPGA)
  Expected hash: 000000000000000000017588478b3612...

[INIT] Initializing Pico Miner HLS IP...
[INIT] Pico Miner IP ready.

============================================================
  STEP 1: Preparing block header
============================================================
  Block header: 80 bytes, byte-swapped to big-endian
  Midstate (SHA-256 after chunk 1):
    XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
  Chunk 2 tail: [XXXXXXXX, XXXXXXXX, XXXXXXXX]
  Known nonce (LE): 0x1E740A08  (BE): 0x080A741E

============================================================
  STEP 2: Mining Block #939260 (full brute-force from nonce=0)
============================================================
  Searching 135,888,926 nonces in chunks of 1000000...

  [MINING] nonces:    1000000 / 135888926  |   0%  |  1.3s  |  781 KH/s
  [MINING] nonces:    2000000 / 135888926  |   1%  |  2.6s  |  781 KH/s
  ...
  [MINING] nonces:  134000000 / 135888926  |  98%  |  171.5s |  781 KH/s
  >> NONCE FOUND! <<
  Found nonce:    0x080A741E (BE)  0x1E740A08 (LE)
  Total searched: 134,888,926 nonces
  Elapsed time:   172.7 seconds
  Hash rate:      781,024 H/s (781.0 KH/s)

============================================================
  STEP 3: Verification
============================================================
  [HW] Found nonce (BE): 0x080A741E
  [SW] Verification:     VALID
  [SW] Double-SHA-256 hash (display order):
    000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06
  [EXP] Expected hash:
    000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06

  Nonce match:  HW=0x080A741E  Known=0x080A741E  [OK]

############################################################
#                                                          #
#                   MINING COMPLETE                        #
#                                                          #
############################################################

  Block:         #939260 (March 4, 2026)
  Difficulty:    144,398,401,518,101
  Nonce found:   0x080A741E (BE) / 0x1E740A08 (LE)
  SW verified:   YES
  Hash valid:    YES

  LEDs: all ON = block successfully mined
```

## HLS Optimization Strategy

### SHA-256 Compress Function

The core `sha256_compress()` function implements the 64-round SHA-256 compression:

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
| `src/pico_miner_tb.cpp` | HLS testbench: 5 tests -- NIST vector, Block 1 (full-range SW + HW), Block 939260 HW, no-solution |
| `src/pico_miner_arm.c` | ARM driver: Block 939260 full-range demo with chunked HW calls, LED progress, XTime timing |
| `doc/poc.tex` | Project memory (bilingual ES/EN): SHA-256 algorithm, architecture, verification, results |
| `run_hls.tcl` | TCL script: builds 2 solutions (II=1 baseline, II=2 relaxed), runs csim/csynth/cosim/export |

## License

This project is for educational purposes.
