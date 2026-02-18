/* =============================================================================
 * Pico Miner -- HLS Testbench
 *
 * Description:
 *   C-simulation testbench for the Pico Miner HLS IP. Contains a software
 *   golden model (pico_miner_sw) that implements the same algorithm as the
 *   HLS function. Both are run with identical inputs and their outputs are
 *   compared to verify correctness.
 *
 *   This testbench is used for:
 *   - Vivado HLS C Simulation (csim)
 *   - Vivado HLS C/RTL Co-Simulation (cosim)
 *
 * Methodology: Follows the same SW-vs-HW comparison pattern used in the
 *              course examples (vector_operator_tb.cpp, reorder_tb.cpp).
 *
 * Author: Pico Miner Project
 * Date: 2026
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "pico_miner.h"

/* =============================================================================
 * Software Reference Implementation (Golden Model)
 *
 * This is a pure-C implementation of the same algorithm. It has NO HLS
 * pragmas and is used purely for verification. If the HLS function produces
 * the same results as this function, the synthesis is correct.
 * =============================================================================
 */

/* Software version of PicoHash (identical logic, no HLS pragmas) */
static unsigned int pico_hash_sw(unsigned int data[BLOCK_HEADER_SIZE],
                                 unsigned int nonce)
{
    unsigned int h = HASH_SEED;
    int i;

    for (i = 0; i < BLOCK_HEADER_SIZE; i++) {
        h = h ^ data[i];
        h = h * FNV_PRIME;
        h = h ^ (h >> 16);
    }

    h = h ^ nonce;
    h = h * MURMUR_M;
    h = h ^ (h >> 13);
    h = h * MURMUR_M;
    h = h ^ (h >> 15);

    return h;
}

/* Software version of the miner (identical logic, no HLS pragmas) */
static void pico_miner_sw(unsigned int block_header[BLOCK_HEADER_SIZE],
                           unsigned int difficulty_target,
                           unsigned int nonce_start,
                           unsigned int nonce_end,
                           unsigned int *found_nonce,
                           unsigned int *found_hash,
                           unsigned int *status)
{
    unsigned int nonce;
    unsigned int hash_result;

    *found_nonce = 0;
    *found_hash  = 0xFFFFFFFF;
    *status      = MINING_NOT_FOUND;

    for (nonce = nonce_start; nonce < nonce_end; nonce++) {
        hash_result = pico_hash_sw(block_header, nonce);
        if (hash_result < difficulty_target) {
            *found_nonce = nonce;
            *found_hash  = hash_result;
            *status      = MINING_FOUND;
            break;
        }
    }
}

/* =============================================================================
 * Helper: Print a separator line
 * =============================================================================
 */
static void print_separator(void)
{
    printf("================================================================\n");
}

/* =============================================================================
 * Test Case 1: Easy difficulty -- should find a nonce quickly
 * =============================================================================
 */
static int test_easy_difficulty(void)
{
    unsigned int block_header[BLOCK_HEADER_SIZE] = {
        0xDEADBEEF,  /* Simulated previous block hash (word 0) */
        0xCAFEBABE,  /* Simulated transaction root (word 1) */
        0x12345678,  /* Simulated timestamp (word 2) */
        0xABCD0001   /* Simulated block version (word 3) */
    };

    /* Easy difficulty: hash must be < 0x10000000 (top 4 bits must be 0000) */
    unsigned int difficulty_target = 0x10000000;
    unsigned int nonce_start = 0;
    unsigned int nonce_end   = 0x00100000;  /* Search up to ~1M nonces */

    /* SW outputs */
    unsigned int sw_nonce, sw_hash, sw_status;
    /* HW outputs */
    unsigned int hw_nonce, hw_hash, hw_status;

    int errors = 0;

    printf("\n");
    print_separator();
    printf("TEST 1: Easy Difficulty (target = 0x%08X)\n", difficulty_target);
    print_separator();

    /* Run software reference */
    printf("[SW] Mining nonces [0x%08X, 0x%08X)...\n", nonce_start, nonce_end);
    pico_miner_sw(block_header, difficulty_target, nonce_start, nonce_end,
                  &sw_nonce, &sw_hash, &sw_status);

    if (sw_status == MINING_FOUND) {
        printf("[SW] FOUND! Nonce=0x%08X  Hash=0x%08X\n", sw_nonce, sw_hash);
    } else {
        printf("[SW] Not found in range.\n");
    }

    /* Run HLS hardware function */
    printf("[HW] Mining nonces [0x%08X, 0x%08X)...\n", nonce_start, nonce_end);
    pico_miner(block_header, difficulty_target, nonce_start, nonce_end,
               &hw_nonce, &hw_hash, &hw_status);

    if (hw_status == MINING_FOUND) {
        printf("[HW] FOUND! Nonce=0x%08X  Hash=0x%08X\n", hw_nonce, hw_hash);
    } else {
        printf("[HW] Not found in range.\n");
    }

    /* Compare results */
    printf("\n--- Comparison ---\n");
    printf("  Status:  SW=%u  HW=%u  %s\n", sw_status, hw_status,
           (sw_status == hw_status) ? "[OK]" : "[MISMATCH]");
    printf("  Nonce:   SW=0x%08X  HW=0x%08X  %s\n", sw_nonce, hw_nonce,
           (sw_nonce == hw_nonce) ? "[OK]" : "[MISMATCH]");
    printf("  Hash:    SW=0x%08X  HW=0x%08X  %s\n", sw_hash, hw_hash,
           (sw_hash == hw_hash) ? "[OK]" : "[MISMATCH]");

    if (sw_status != hw_status) errors++;
    if (sw_nonce  != hw_nonce)  errors++;
    if (sw_hash   != hw_hash)   errors++;

    /* Verify the hash is actually below target */
    if (hw_status == MINING_FOUND && hw_hash >= difficulty_target) {
        printf("  ERROR: Hash 0x%08X is NOT below target 0x%08X!\n",
               hw_hash, difficulty_target);
        errors++;
    }

    return errors;
}

/* =============================================================================
 * Test Case 2: Medium difficulty
 * =============================================================================
 */
static int test_medium_difficulty(void)
{
    unsigned int block_header[BLOCK_HEADER_SIZE] = {
        0x01020304,
        0x05060708,
        0x090A0B0C,
        0x0D0E0F10
    };

    /* Medium difficulty: hash must be < 0x00100000 (top 12 bits must be 0) */
    unsigned int difficulty_target = 0x00100000;
    unsigned int nonce_start = 0;
    unsigned int nonce_end   = 0x01000000;  /* Search up to ~16M nonces */

    unsigned int sw_nonce, sw_hash, sw_status;
    unsigned int hw_nonce, hw_hash, hw_status;
    int errors = 0;

    printf("\n");
    print_separator();
    printf("TEST 2: Medium Difficulty (target = 0x%08X)\n", difficulty_target);
    print_separator();

    printf("[SW] Mining nonces [0x%08X, 0x%08X)...\n", nonce_start, nonce_end);
    pico_miner_sw(block_header, difficulty_target, nonce_start, nonce_end,
                  &sw_nonce, &sw_hash, &sw_status);

    if (sw_status == MINING_FOUND) {
        printf("[SW] FOUND! Nonce=0x%08X  Hash=0x%08X\n", sw_nonce, sw_hash);
    } else {
        printf("[SW] Not found in range.\n");
    }

    printf("[HW] Mining nonces [0x%08X, 0x%08X)...\n", nonce_start, nonce_end);
    pico_miner(block_header, difficulty_target, nonce_start, nonce_end,
               &hw_nonce, &hw_hash, &hw_status);

    if (hw_status == MINING_FOUND) {
        printf("[HW] FOUND! Nonce=0x%08X  Hash=0x%08X\n", hw_nonce, hw_hash);
    } else {
        printf("[HW] Not found in range.\n");
    }

    printf("\n--- Comparison ---\n");
    printf("  Status:  SW=%u  HW=%u  %s\n", sw_status, hw_status,
           (sw_status == hw_status) ? "[OK]" : "[MISMATCH]");
    printf("  Nonce:   SW=0x%08X  HW=0x%08X  %s\n", sw_nonce, hw_nonce,
           (sw_nonce == hw_nonce) ? "[OK]" : "[MISMATCH]");
    printf("  Hash:    SW=0x%08X  HW=0x%08X  %s\n", sw_hash, hw_hash,
           (sw_hash == hw_hash) ? "[OK]" : "[MISMATCH]");

    if (sw_status != hw_status) errors++;
    if (sw_nonce  != hw_nonce)  errors++;
    if (sw_hash   != hw_hash)   errors++;

    if (hw_status == MINING_FOUND && hw_hash >= difficulty_target) {
        printf("  ERROR: Hash 0x%08X is NOT below target 0x%08X!\n",
               hw_hash, difficulty_target);
        errors++;
    }

    return errors;
}

/* =============================================================================
 * Test Case 3: No valid nonce in range (difficulty too hard for given range)
 * =============================================================================
 */
static int test_no_solution(void)
{
    unsigned int block_header[BLOCK_HEADER_SIZE] = {
        0xAAAAAAAA,
        0xBBBBBBBB,
        0xCCCCCCCC,
        0xDDDDDDDD
    };

    /* Extremely hard: hash must be < 1 (essentially impossible) */
    unsigned int difficulty_target = 0x00000001;
    unsigned int nonce_start = 0;
    unsigned int nonce_end   = 1000;  /* Only try 1000 nonces */

    unsigned int sw_nonce, sw_hash, sw_status;
    unsigned int hw_nonce, hw_hash, hw_status;
    int errors = 0;

    printf("\n");
    print_separator();
    printf("TEST 3: Impossible Difficulty (target = 0x%08X, range = %u)\n",
           difficulty_target, nonce_end - nonce_start);
    print_separator();

    printf("[SW] Mining nonces [0x%08X, 0x%08X)...\n", nonce_start, nonce_end);
    pico_miner_sw(block_header, difficulty_target, nonce_start, nonce_end,
                  &sw_nonce, &sw_hash, &sw_status);
    printf("[SW] Status: %s\n",
           (sw_status == MINING_FOUND) ? "FOUND" : "NOT FOUND (expected)");

    printf("[HW] Mining nonces [0x%08X, 0x%08X)...\n", nonce_start, nonce_end);
    pico_miner(block_header, difficulty_target, nonce_start, nonce_end,
               &hw_nonce, &hw_hash, &hw_status);
    printf("[HW] Status: %s\n",
           (hw_status == MINING_FOUND) ? "FOUND" : "NOT FOUND (expected)");

    printf("\n--- Comparison ---\n");
    printf("  Status:  SW=%u  HW=%u  %s\n", sw_status, hw_status,
           (sw_status == hw_status) ? "[OK]" : "[MISMATCH]");

    if (sw_status != hw_status) errors++;

    /* Both should report NOT FOUND */
    if (hw_status != MINING_NOT_FOUND) {
        printf("  ERROR: Expected NOT FOUND but HW reported FOUND!\n");
        errors++;
    }

    return errors;
}

/* =============================================================================
 * Test Case 4: Hash function verification (specific known values)
 * =============================================================================
 */
static int test_hash_determinism(void)
{
    unsigned int block_header[BLOCK_HEADER_SIZE] = {0, 0, 0, 0};
    unsigned int hash_a, hash_b;
    int errors = 0;

    printf("\n");
    print_separator();
    printf("TEST 4: Hash Determinism and Avalanche Check\n");
    print_separator();

    /* Same input must produce same output */
    hash_a = pico_hash_sw(block_header, 0);
    hash_b = pico_hash_sw(block_header, 0);
    printf("  Hash(zeros, nonce=0): 0x%08X  (repeat: 0x%08X)  %s\n",
           hash_a, hash_b, (hash_a == hash_b) ? "[OK]" : "[MISMATCH]");
    if (hash_a != hash_b) errors++;

    /* Different nonces should produce different hashes */
    hash_a = pico_hash_sw(block_header, 0);
    hash_b = pico_hash_sw(block_header, 1);
    printf("  Hash(zeros, nonce=0)=0x%08X  vs  Hash(zeros, nonce=1)=0x%08X\n",
           hash_a, hash_b);
    printf("  Different? %s\n", (hash_a != hash_b) ? "[OK]" : "[FAIL - same!]");
    if (hash_a == hash_b) errors++;

    /* Small data change should produce very different hash (avalanche) */
    unsigned int header_a[BLOCK_HEADER_SIZE] = {0, 0, 0, 0};
    unsigned int header_b[BLOCK_HEADER_SIZE] = {1, 0, 0, 0};  /* 1-bit change */
    hash_a = pico_hash_sw(header_a, 42);
    hash_b = pico_hash_sw(header_b, 42);
    printf("  Hash([0,0,0,0], 42)=0x%08X  vs  Hash([1,0,0,0], 42)=0x%08X\n",
           hash_a, hash_b);

    /* Count differing bits (hamming distance) */
    unsigned int diff = hash_a ^ hash_b;
    int bit_count = 0;
    while (diff) { bit_count += diff & 1; diff >>= 1; }
    printf("  Hamming distance: %d / 32 bits (ideal ~16)\n", bit_count);
    /* A good hash should flip ~half the bits; warn if fewer than 8 */
    if (bit_count < 8) {
        printf("  WARNING: Poor avalanche behavior!\n");
    } else {
        printf("  Avalanche: [OK]\n");
    }

    return errors;
}

/* =============================================================================
 * Main: Run all tests
 * =============================================================================
 */
int main(void)
{
    int total_errors = 0;

    printf("\n");
    printf("################################################################\n");
    printf("#                                                              #\n");
    printf("#             PICO MINER -- HLS Testbench                     #\n");
    printf("#         FPGA-Based Proof of Work Mining Accelerator          #\n");
    printf("#                                                              #\n");
    printf("################################################################\n");

    total_errors += test_easy_difficulty();
    total_errors += test_medium_difficulty();
    total_errors += test_no_solution();
    total_errors += test_hash_determinism();

    printf("\n");
    print_separator();
    if (total_errors == 0) {
        printf("ALL TESTS PASSED -- SW and HW outputs match perfectly.\n");
    } else {
        printf("FAILED: %d error(s) detected.\n", total_errors);
    }
    print_separator();
    printf("\n");

    return total_errors;
}
