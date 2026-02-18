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
#pragma HLS INLINE
    unsigned int h = HASH_SEED;
    int i;

    /* Absorption phase: mix in each data word */
    hash_absorb_loop:
    for (i = 0; i < BLOCK_HEADER_SIZE; i++) {
#pragma HLS UNROLL
        h = h ^ data[i];
        h = h * FNV_PRIME;
        h = h ^ (h >> 16);
    }

    return h;
}

/* Finalize hash for one nonce from the precomputed header state. */
static unsigned int pico_hash_nonce(unsigned int header_state,
                                    unsigned int nonce)
{
#pragma HLS INLINE
    unsigned int h = header_state;

    /* Incorporate nonce */
    h = h ^ nonce;

    /* Finalization (MurmurHash-style bit mixing) */
    h = h * MURMUR_M;
    h = h ^ (h >> 13);
    h = h * MURMUR_M;
    h = h ^ (h >> 15);

    return h;
}

/* -----------------------------------------------------------------------------
 * pico_miner: Top-level HLS function (synthesized to hardware IP)
 *
 * Iterates through nonce values [nonce_start, nonce_end), computing the
 * PicoHash for each. If a hash below the difficulty target is found,
 * the winning nonce and hash are returned and the search terminates early.
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
    unsigned int hash_result;
    unsigned int best_nonce = 0;
    unsigned int best_hash  = 0xFFFFFFFF;
    unsigned int mining_status = MINING_NOT_FOUND;
    unsigned int header_state = pico_hash_header_prefix(block_header);

    /* --- Main mining loop: iterate through nonce range --- */
    mining_loop:
    for (nonce = nonce_start; nonce < nonce_end; nonce++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=16777216

        /* Compute hash for this nonce */
        hash_result = pico_hash_nonce(header_state, nonce);

        /* Check if hash meets difficulty target */
        if (hash_result < difficulty_target) {
            best_nonce = nonce;
            best_hash  = hash_result;
            mining_status = MINING_FOUND;
            break;  /* Found a valid nonce -- stop searching */
        }
    }

    /* Write results to output ports */
    *found_nonce = best_nonce;
    *found_hash  = best_hash;
    *status      = mining_status;
}
