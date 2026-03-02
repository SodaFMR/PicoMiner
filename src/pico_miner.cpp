/* =============================================================================
 * Pico Miner -- FPGA-Based Proof of Work Mining Accelerator
 * HLS Source File
 *
 * Description:
 *   This module implements a simplified Proof of Work (PoW) miner for
 *   educational purposes. It uses a custom lightweight hash function
 *   (PicoHash) to search for a nonce that produces a hash below a given
 *   difficulty target -- the same fundamental operation performed by
 *   Bitcoin miners, but with a simpler hash function.
 *
 * Target: Xilinx Zynq-7020 (xc7z020clg484-1) via Vivado HLS 2019.1
 * Interface: AXI-Lite (s_axilite) for all ports
 *
 * Author: Pico Miner Project
 * Date: 2026
 * =============================================================================
 */

#include "pico_miner.h"

/* -----------------------------------------------------------------------------
 * PicoHash: Custom lightweight hash function
 *
 * Inspired by FNV-1a and MurmurHash finalizer.
 * - Absorption phase: XOR each data word, multiply by FNV prime, mix bits
 * - Finalization: Incorporate nonce, apply MurmurHash-style bit mixing
 *
 * This is NOT cryptographically secure (32-bit output), but demonstrates
 * the hash-and-compare pattern used in real PoW mining.
 * -------------------------------------------------------------------------- */
/* Precompute header contribution once; it does not depend on nonce. */
static unsigned int pico_hash_header_prefix(
    const unsigned int data[BLOCK_HEADER_SIZE])
{
#pragma HLS INLINE off
    unsigned int h = HASH_SEED;
    int i;

    /* Absorption phase: mix in each data word */
    hash_absorb_loop:
    for (i = 0; i < BLOCK_HEADER_SIZE; i++) {
        h = h ^ data[i];
        h = h * FNV_PRIME;
        h = h ^ (h >> 16);
    }

    return h;
}

/* --------------------------------------------------------------------------
 * Nonce finalization -- split into two inline stages.
 *
 * On Zynq-7020 (-1 speed grade) a 32x32 multiply takes ~5-6 ns.  Chaining
 * two multiplies plus XOR-shifts plus the difficulty comparison exceeds the
 * 10 ns budget (WNS was -1.4 ns in earlier builds).  By marking each stage
 * as INLINE, HLS is free to place a pipeline register between them when the
 * mining loop is pipelined, keeping each cycle well under 10 ns.
 * -------------------------------------------------------------------------- */

/* Stage 1: XOR nonce into header state, first multiply + shift. */
static unsigned int pico_hash_nonce_s1(unsigned int header_state,
                                       unsigned int nonce)
{
#pragma HLS INLINE
    unsigned int h = header_state ^ nonce;
    h = h * MURMUR_M;
    h = h ^ (h >> 13);
    return h;
}

/* Stage 2: second multiply + final shift. */
static unsigned int pico_hash_nonce_s2(unsigned int h)
{
#pragma HLS INLINE
    h = h * MURMUR_M;
    h = h ^ (h >> 15);
    return h;
}

/* -----------------------------------------------------------------------------
 * pico_miner: Top-level HLS function (synthesized to hardware IP)
 *
 * Iterates through nonce values [nonce_start, nonce_end), computing the
 * PicoHash for each. If a hash below the difficulty target is found,
 * the first winning nonce/hash pair is captured and returned at the end.
 *
 * All ports use AXI-Lite interface bundled into "myaxi" for PS/PL
 * communication on the Zynq.
 * -------------------------------------------------------------------------- */
void pico_miner(unsigned int block_header[BLOCK_HEADER_SIZE],
                unsigned int difficulty_target,
                unsigned int nonce_start,
                unsigned int nonce_end,
                unsigned int *found_nonce,
                unsigned int *found_hash,
                unsigned int *status)
{
    /* AXI-Lite interface pragmas (same pattern as course examples) */
#pragma HLS INTERFACE s_axilite port=block_header    bundle=myaxi
#pragma HLS INTERFACE s_axilite port=difficulty_target bundle=myaxi
#pragma HLS INTERFACE s_axilite port=nonce_start     bundle=myaxi
#pragma HLS INTERFACE s_axilite port=nonce_end       bundle=myaxi
#pragma HLS INTERFACE s_axilite port=found_nonce     bundle=myaxi
#pragma HLS INTERFACE s_axilite port=found_hash      bundle=myaxi
#pragma HLS INTERFACE s_axilite port=status          bundle=myaxi
#pragma HLS INTERFACE s_axilite port=return          bundle=myaxi

    /* Allow HLS to read all header words in parallel */
#pragma HLS ARRAY_PARTITION variable=block_header complete dim=1

    unsigned int nonce;
    unsigned int hash_s1;
    unsigned int hash_result;
    unsigned int best_nonce = 0;
    unsigned int best_hash  = 0xFFFFFFFF;
    unsigned int mining_status = MINING_NOT_FOUND;
    unsigned int found = 0;
    unsigned int header_state = pico_hash_header_prefix(block_header);

    /* --- Main mining loop: iterate through nonce range ---
     *
     * The hash computation is split into two inline stages so that HLS
     * can insert a pipeline register between the two 32x32 multiplies.
     * This keeps each pipeline stage under 10 ns on Zynq-7020 (-1).
     */
    mining_loop:
    for (nonce = nonce_start; nonce < nonce_end; nonce++) {
#pragma HLS PIPELINE II=2
#pragma HLS LOOP_TRIPCOUNT min=1 max=16777216

        /* Stage 1: XOR nonce + first multiply/shift */
        hash_s1 = pico_hash_nonce_s1(header_state, nonce);

        /* Stage 2: second multiply/shift */
        hash_result = pico_hash_nonce_s2(hash_s1);

        /* Capture only the first valid nonce to avoid loop-exit control
         * pressure (no early exit -- lets HLS pipeline freely). */
        if (!found && (hash_result < difficulty_target)) {
            best_nonce = nonce;
            best_hash  = hash_result;
            mining_status = MINING_FOUND;
            found = 1;
        }
    }

    /* Write results to output ports */
    *found_nonce = best_nonce;
    *found_hash  = best_hash;
    *status      = mining_status;
}
