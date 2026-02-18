# Pico Miner

**FPGA-Based Proof of Work Mining Accelerator**

A proof-of-concept project that demonstrates cryptocurrency mining (Proof of Work) accelerated on FPGA hardware using Xilinx Vivado HLS. Designed for the Zynq-7020 SoC (ZedBoard).

---

## Overview

Cryptocurrency mining relies on a brute-force search for a **nonce** value that, when hashed with the block data, produces a hash below a **difficulty target**. This is the core Proof of Work (PoW) mechanism used in Bitcoin and similar blockchains.

**Pico Miner** implements this fundamental operation on FPGA hardware:

- A custom lightweight hash function (**PicoHash**) replaces SHA-256 for simplicity
- The nonce search loop runs in dedicated FPGA hardware, synthesized from C++ via Vivado HLS
- An ARM processor (Zynq PS) controls the hardware accelerator via AXI-Lite
- HLS optimization directives (pipeline, array partition) demonstrate hardware acceleration techniques

> For a detailed explanation of the design decisions and comparison with real Bitcoin mining, see the [Proof of Concept document](doc/poc.tex).

## Project Structure

```
Pico_Miner/
├── src/                               # Our project implementation
│   ├── pico_miner.h                   #   Shared header (constants, function prototype)
│   ├── pico_miner.cpp                 #   HLS source (synthesized to hardware)
│   ├── pico_miner_tb.cpp              #   HLS testbench (C simulation & co-simulation)
│   └── pico_miner_arm.c               #   ARM driver (runs on Zynq PS)
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

### The Mining Algorithm

1. **Input**: Block header (4 x 32-bit words), difficulty target, nonce range
2. **Process**: For each nonce in the range:
   - Compute `hash = PicoHash(block_header, nonce)`
   - If `hash < difficulty_target`: mining successful, return the nonce
3. **Output**: Winning nonce, its hash value, and a found/not-found status

### PicoHash Function

PicoHash is a custom 32-bit hash inspired by FNV-1a and MurmurHash:

- **Absorption phase**: Iterates over data words using XOR + FNV prime multiplication + bit mixing
- **Finalization**: Incorporates the nonce and applies MurmurHash-style bit shuffling

It is **not cryptographically secure** (32-bit output), but demonstrates the same hash-and-compare pattern used in real PoW mining. See `doc/poc.tex` for the full algorithm specification.

### System Architecture

```
┌──────────────────────┐     AXI-Lite      ┌──────────────────────┐
│   ARM Cortex-A9      │◄──────────────────►│   Pico Miner IP      │
│   (Zynq PS)          │                    │   (Zynq PL)          │
│                      │  Write: header,    │                      │
│   pico_miner_arm.c   │  target, nonce     │   pico_miner.cpp     │
│   - Configure miner  │  range             │   - Hash computation │
│   - Start/poll/read  │                    │   - Nonce iteration  │
│   - SW verification  │  Read: nonce,      │   - Target comparison│
│                      │  hash, status      │                      │
└──────────────────────┘                    └──────────────────────┘
```

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
# 4. Set target part: xc7z020clg484-1, clock: 10ns
# 5. Run C Simulation -> should print "ALL TESTS PASSED"
# 6. Run C Synthesis -> generates RTL + performance report
# 7. Run C/RTL Co-Simulation -> verifies RTL matches C behavior
# 8. Export RTL (IP Catalog format)
```

### Step 2: Vivado -- Create Block Design

1. Open Vivado 2019.1, create a new project targeting `xc7z020clg484-1`
2. Create a Block Design
3. Add the **ZYNQ7 Processing System** IP
4. Add the **Pico Miner** IP (from the HLS export, add the IP repository)
5. Run **Connection Automation** (connects AXI-Lite automatically)
6. **Generate Bitstream**
7. **Export Hardware** (File > Export > Export Hardware, include bitstream)
8. **Launch SDK** (File > Launch SDK)

### Step 3: Xilinx SDK -- ARM Software

1. Create a new **Application Project** (Hello World template)
2. Replace the generated `helloworld.c` with `src/pico_miner_arm.c`
3. Build the project
4. Program the FPGA and run the application
5. Observe results on the UART serial terminal (115200 baud)

## Expected Output

### C Simulation (Vivado HLS)

```
################################################################
#                                                              #
#             PICO MINER -- HLS Testbench                     #
#         FPGA-Based Proof of Work Mining Accelerator          #
#                                                              #
################################################################

================================================================
TEST 1: Easy Difficulty (target = 0x10000000)
================================================================
[SW] Mining nonces [0x00000000, 0x00100000)...
[SW] FOUND! Nonce=0x000000XX  Hash=0x0XXXXXXX
[HW] Mining nonces [0x00000000, 0x00100000)...
[HW] FOUND! Nonce=0x000000XX  Hash=0x0XXXXXXX

--- Comparison ---
  Status:  SW=1  HW=1  [OK]
  Nonce:   SW=0x000000XX  HW=0x000000XX  [OK]
  Hash:    SW=0x0XXXXXXX  HW=0x0XXXXXXX  [OK]

...

================================================================
ALL TESTS PASSED -- SW and HW outputs match perfectly.
================================================================
```

*(Exact nonce and hash values depend on the hash function -- the important thing is that SW and HW match.)*

## HLS Optimization Results

The project includes pragmas for loop pipelining and array partitioning. Expected synthesis results at 100 MHz:

| Configuration | Latency/Hash | Initiation Interval | Throughput |
|---|---|---|---|
| Baseline (no directives) | ~20 cycles | ~20 | ~5 MH/s |
| Loop pipelining (II=1) | ~20 cycles | 1 | ~100 MH/s |
| + Array partition | ~10 cycles | 1 | ~100 MH/s |

These results can be compared using Vivado HLS "Compare Solutions" feature, similar to the approach in `examples/readme.txt`.

## Comparison with Real Bitcoin Mining

| Aspect | Bitcoin | Pico Miner |
|---|---|---|
| Hash function | Double SHA-256 | PicoHash (custom 32-bit) |
| Block header | 80 bytes | 16 bytes (4 words) |
| Nonce | 32 bits | 32 bits |
| Target comparison | 256-bit | 32-bit |
| Core principle | Brute-force PoW | Brute-force PoW |
| HW acceleration | ASICs / FPGAs | FPGA (Zynq HLS) |

The fundamental mining loop -- hash, compare, increment nonce -- is identical. Only the hash complexity and data sizes differ.

## File Descriptions

| File | Purpose |
|---|---|
| `src/pico_miner.h` | Header with constants and function prototype |
| `src/pico_miner.cpp` | HLS source: hash function + nonce search loop (synthesized to hardware) |
| `src/pico_miner_tb.cpp` | HLS testbench: SW golden model + 4 test cases |
| `src/pico_miner_arm.c` | ARM driver: configures HLS IP via AXI-Lite, runs mining, verifies results |
| `doc/poc.tex` | LaTeX proof of concept document with full algorithm specification |
| `run_hls.tcl` | TCL script to automate Vivado HLS project creation and synthesis |

## License

This project is for educational purposes.
