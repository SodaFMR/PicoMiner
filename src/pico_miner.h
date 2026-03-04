/* =============================================================================
 * Pico Miner -- Header File
 *
 * Shared definitions for the HLS source, testbench, and ARM driver.
 *
 * This version implements real Bitcoin double-SHA-256 mining with midstate
 * optimization, targeting the Zynq-7020 via Vivado HLS 2019.1.
 * =============================================================================
 */

#ifndef PICO_MINER_H
#define PICO_MINER_H

/* ---- Bitcoin block header layout ----------------------------------------
 *
 * The 80-byte header is split for the midstate optimization:
 *
 *   Chunk 1 (64 bytes = 16 words): version + prevhash + merkle[0..27]
 *     -> processed once on ARM to produce the midstate (8 words)
 *
 *   Chunk 2 (16 bytes = 4 words):  merkle[28..31] + timestamp + bits + nonce
 *     -> the FPGA iterates the nonce word while the other 3 stay fixed
 *
 * All words inside SHA-256 use big-endian 32-bit representation.
 * -------------------------------------------------------------------------- */
#define MIDSTATE_WORDS    8     /* SHA-256 intermediate state after chunk 1  */
#define CHUNK2_TAIL_WORDS 3     /* chunk 2 without nonce (merkle_tail,ts,bits)*/
#define SHA256_HASH_WORDS 8     /* full SHA-256 digest: 8 x 32-bit words    */

/* Mining status codes */
#define MINING_FOUND      1
#define MINING_NOT_FOUND  0

/* ---- SHA-256 initial hash values (FIPS 180-4 section 5.3.3) ------------ */
#define SHA256_H0  0x6a09e667u
#define SHA256_H1  0xbb67ae85u
#define SHA256_H2  0x3c6ef372u
#define SHA256_H3  0xa54ff53au
#define SHA256_H4  0x510e527fu
#define SHA256_H5  0x9b05688cu
#define SHA256_H6  0x1f83d9abu
#define SHA256_H7  0x5be0cd19u

/* ---- SHA-256 round constants (K[0..63]) -------------------------------- */
static const unsigned int SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

/* ---- Function prototype ------------------------------------------------ */
void pico_miner(
    /* Precomputed midstate from chunk 1 (computed by ARM) */
    unsigned int midstate[MIDSTATE_WORDS],
    /* Last 3 words of chunk 2: merkle_tail, timestamp, bits */
    unsigned int chunk2_tail[CHUNK2_TAIL_WORDS],
    /* Nonce search range [nonce_start, nonce_end) */
    unsigned int nonce_start,
    unsigned int nonce_end,
    /* Difficulty target: compared against final_hash[7], the last word of
     * the SHA-256 output, which corresponds to the most-significant 32 bits
     * of the Bitcoin display hash (reversed byte order).
     * For early Bitcoin blocks, target_hi = 0x00000000 requires the
     * display hash to start with 32 zero bits. */
    unsigned int target_hi,
    /* Outputs */
    unsigned int *found_nonce,
    unsigned int *status);

#endif /* PICO_MINER_H */
