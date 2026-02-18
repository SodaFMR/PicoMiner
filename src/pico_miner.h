/* =============================================================================
 * Pico Miner -- Header File
 *
 * Shared definitions for the HLS source, testbench, and ARM driver.
 * =============================================================================
 */

#ifndef PICO_MINER_H
#define PICO_MINER_H

/* Block header size: 4 x 32-bit words (simplified from Bitcoin's 80 bytes) */
#define BLOCK_HEADER_SIZE  4

/* PicoHash constants */
#define HASH_SEED    0x5A3CF1E7u   /* Initial hash state */
#define FNV_PRIME    0x01000193u   /* FNV-1a prime (32-bit) */
#define MURMUR_M     0x5BD1E995u   /* MurmurHash finalizer constant */

/* Mining status codes */
#define MINING_FOUND      1
#define MINING_NOT_FOUND  0

/* Default nonce search range (used as LOOP_TRIPCOUNT hint for HLS reports) */
#define DEFAULT_NONCE_RANGE  0x01000000u  /* 16 million nonces */

/* Function prototype for the HLS top-level function */
void pico_miner(unsigned int block_header[BLOCK_HEADER_SIZE],
                unsigned int difficulty_target,
                unsigned int nonce_start,
                unsigned int nonce_end,
                unsigned int *found_nonce,
                unsigned int *found_hash,
                unsigned int *status);

#endif /* PICO_MINER_H */
