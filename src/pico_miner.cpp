/* =============================================================================
 * Pico Miner -- FPGA-Based Bitcoin Mining Accelerator
 * HLS Source File
 *
 * Description:
 *   Implements real Bitcoin double-SHA-256 mining with midstate optimization.
 *   The ARM processor precomputes the midstate (SHA-256 state after the first
 *   64-byte chunk of the 80-byte block header).  The FPGA iterates nonces,
 *   completing the second SHA-256 chunk and running the second full SHA-256,
 *   then checks the result against the difficulty target.
 *
 * Target: Xilinx Zynq-7020 (xc7z020clg484-1) via Vivado HLS 2019.1
 * Interface: AXI-Lite (s_axilite) for all ports
 *
 * Author: Pico Miner Project
 * Date: 2026
 * =============================================================================
 */

#include "pico_miner.h"

/* ---- SHA-256 helper macros --------------------------------------------- */
#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n)   ((x) >> (n))

#define CH(e, f, g)   (((e) & (f)) ^ ((~(e)) & (g)))
#define MAJ(a, b, c)  (((a) & (b)) ^ ((a) & (c)) ^ ((b) & (c)))

#define SIGMA0(a)  (ROTR(a, 2)  ^ ROTR(a, 13) ^ ROTR(a, 22))
#define SIGMA1(e)  (ROTR(e, 6)  ^ ROTR(e, 11) ^ ROTR(e, 25))
#define sigma0(x)  (ROTR(x, 7)  ^ ROTR(x, 18) ^ SHR(x, 3))
#define sigma1(x)  (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

/* ---- SHA-256 compression: process one 64-byte block -------------------- */
/* Takes 8-word initial state and 16 message words, returns updated state.  */
static void sha256_compress(const unsigned int state_in[8],
                            const unsigned int W_in[16],
                            unsigned int state_out[8])
{
#pragma HLS INLINE off
    unsigned int W[64];
#pragma HLS ARRAY_PARTITION variable=W complete dim=1
    unsigned int a, b, c, d, e, f, g, h;
    int i;

    /* Load first 16 words */
    load_W:
    for (i = 0; i < 16; i++) {
#pragma HLS UNROLL
        W[i] = W_in[i];
    }

    /* Expand message schedule */
    expand_W:
    for (i = 16; i < 64; i++) {
#pragma HLS UNROLL
        W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];
    }

    /* Initialise working variables */
    a = state_in[0]; b = state_in[1]; c = state_in[2]; d = state_in[3];
    e = state_in[4]; f = state_in[5]; g = state_in[6]; h = state_in[7];

    /* 64 compression rounds */
    compress:
    for (i = 0; i < 64; i++) {
#pragma HLS PIPELINE II=1
        unsigned int temp1 = h + SIGMA1(e) + CH(e, f, g) + SHA256_K[i] + W[i];
        unsigned int temp2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    /* Add compressed chunk to state */
    state_out[0] = state_in[0] + a;
    state_out[1] = state_in[1] + b;
    state_out[2] = state_in[2] + c;
    state_out[3] = state_in[3] + d;
    state_out[4] = state_in[4] + e;
    state_out[5] = state_in[5] + f;
    state_out[6] = state_in[6] + g;
    state_out[7] = state_in[7] + h;
}

/* ---- Top-level mining function ----------------------------------------- */
void pico_miner(
    unsigned int midstate[MIDSTATE_WORDS],
    unsigned int chunk2_tail[CHUNK2_TAIL_WORDS],
    unsigned int nonce_start,
    unsigned int nonce_end,
    unsigned int target_hi,
    unsigned int *found_nonce,
    unsigned int *status)
{
    /* ---- AXI-Lite interface pragmas ------------------------------------ */
#pragma HLS INTERFACE s_axilite port=midstate     bundle=myaxi
#pragma HLS INTERFACE s_axilite port=chunk2_tail  bundle=myaxi
#pragma HLS INTERFACE s_axilite port=nonce_start  bundle=myaxi
#pragma HLS INTERFACE s_axilite port=nonce_end    bundle=myaxi
#pragma HLS INTERFACE s_axilite port=target_hi    bundle=myaxi
#pragma HLS INTERFACE s_axilite port=found_nonce  bundle=myaxi
#pragma HLS INTERFACE s_axilite port=status       bundle=myaxi
#pragma HLS INTERFACE s_axilite port=return       bundle=myaxi

    /* ---- Allow parallel access to arrays ------------------------------- */
#pragma HLS ARRAY_PARTITION variable=midstate     complete dim=1
#pragma HLS ARRAY_PARTITION variable=chunk2_tail  complete dim=1

    unsigned int nonce;
    unsigned int result_nonce  = 0;
    unsigned int mining_status = MINING_NOT_FOUND;
    unsigned int found = 0;

    /* Cache midstate and tail in local registers */
    unsigned int ms[8];
#pragma HLS ARRAY_PARTITION variable=ms complete dim=1
    unsigned int tail[3];
#pragma HLS ARRAY_PARTITION variable=tail complete dim=1

    int i;
    cache_ms:
    for (i = 0; i < 8; i++) {
#pragma HLS UNROLL
        ms[i] = midstate[i];
    }
    cache_tail:
    for (i = 0; i < 3; i++) {
#pragma HLS UNROLL
        tail[i] = chunk2_tail[i];
    }

    /* SHA-256 initial hash values (used for the second hash) */
    unsigned int H_init[8];
#pragma HLS ARRAY_PARTITION variable=H_init complete dim=1
    H_init[0] = SHA256_H0; H_init[1] = SHA256_H1;
    H_init[2] = SHA256_H2; H_init[3] = SHA256_H3;
    H_init[4] = SHA256_H4; H_init[5] = SHA256_H5;
    H_init[6] = SHA256_H6; H_init[7] = SHA256_H7;

    /* ---- Main mining loop ---------------------------------------------- */
    mining_loop:
    for (nonce = nonce_start; nonce < nonce_end; nonce++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=16777216
        /* Skip if we already found a valid nonce */
        if (found) continue;

        /* ---- Build chunk 2 message words (16 words total) -------------- *
         * W[0]  = merkle_tail  (tail of merkle root, bytes 28-31)
         * W[1]  = timestamp
         * W[2]  = bits (compact target)
         * W[3]  = nonce          <-- this is what we iterate
         * W[4]  = 0x80000000     (padding: 1-bit after 80 bytes of data)
         * W[5..14] = 0           (padding zeros)
         * W[15] = 0x00000280     (message length = 640 bits, big-endian)
         */
        unsigned int chunk2_W[16];
#pragma HLS ARRAY_PARTITION variable=chunk2_W complete dim=1

        chunk2_W[0]  = tail[0];
        chunk2_W[1]  = tail[1];
        chunk2_W[2]  = tail[2];
        chunk2_W[3]  = nonce;
        chunk2_W[4]  = 0x80000000u;
        chunk2_W[5]  = 0; chunk2_W[6]  = 0; chunk2_W[7]  = 0;
        chunk2_W[8]  = 0; chunk2_W[9]  = 0; chunk2_W[10] = 0;
        chunk2_W[11] = 0; chunk2_W[12] = 0; chunk2_W[13] = 0;
        chunk2_W[14] = 0;
        chunk2_W[15] = 0x00000280u;  /* 640 bits */

        /* ---- First SHA-256: compress chunk 2 with midstate ------------- */
        unsigned int first_hash[8];
#pragma HLS ARRAY_PARTITION variable=first_hash complete dim=1
        sha256_compress(ms, chunk2_W, first_hash);

        /* ---- Build message block for second SHA-256 -------------------- *
         * Input is the 32-byte first_hash, padded to 64 bytes:
         * W[0..7]  = first_hash[0..7]
         * W[8]     = 0x80000000   (padding)
         * W[9..14] = 0
         * W[15]    = 0x00000100   (message length = 256 bits)
         */
        unsigned int hash2_W[16];
#pragma HLS ARRAY_PARTITION variable=hash2_W complete dim=1

        build_hash2_msg:
        for (i = 0; i < 8; i++) {
#pragma HLS UNROLL
            hash2_W[i] = first_hash[i];
        }
        hash2_W[8]  = 0x80000000u;
        hash2_W[9]  = 0; hash2_W[10] = 0; hash2_W[11] = 0;
        hash2_W[12] = 0; hash2_W[13] = 0; hash2_W[14] = 0;
        hash2_W[15] = 0x00000100u;  /* 256 bits */

        /* ---- Second SHA-256: hash of the first hash -------------------- */
        unsigned int final_hash[8];
#pragma HLS ARRAY_PARTITION variable=final_hash complete dim=1
        sha256_compress(H_init, hash2_W, final_hash);

        /* ---- Check difficulty ------------------------------------------- *
         * Bitcoin displays hashes in reversed byte order.  The leading
         * zeros in the display hash correspond to the LAST word of the
         * SHA-256 output (final_hash[7]).
         *
         * For early blocks (difficulty 1), the target requires the hash
         * to start with 32 zero bits in display order, i.e.,
         * final_hash[7] <= target_hi where target_hi = 0x00000000.
         */
        if (final_hash[7] <= target_hi) {
            result_nonce  = nonce;
            mining_status = MINING_FOUND;
            found = 1;
        }
    }

    /* Write results */
    *found_nonce = result_nonce;
    *status      = mining_status;
}
