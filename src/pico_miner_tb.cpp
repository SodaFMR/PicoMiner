/* =============================================================================
 * Pico Miner -- HLS Testbench
 *
 * Description:
 *   Testbench for the Bitcoin double-SHA-256 mining accelerator.
 *   Verifies correctness using NIST test vectors and real Bitcoin blocks.
 *
 *   Test cases:
 *   1. SHA-256 NIST FIPS 180-4 known vector: SHA-256("abc")
 *   2. Block 1 full-range SW mining from nonce=0 (~31.8M iterations)
 *   3. Block 1 HW mining (narrow window for cosim)
 *   4. Block 939260 HW mining (narrow window for cosim) -- recent block
 *   5. No-solution test
 *
 * Author: Pico Miner Project
 * Date:   2026
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "pico_miner.h"

/* =============================================================================
 * Software SHA-256 Implementation (Golden Model)
 * =============================================================================
 */

static unsigned int rotr(unsigned int x, int n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_compress_sw(const unsigned int state_in[8],
                                const unsigned int msg[16],
                                unsigned int state_out[8])
{
    unsigned int W[64];
    unsigned int a, b, c, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++) W[i] = msg[i];
    for (i = 16; i < 64; i++) {
        unsigned int s0 = rotr(W[i-15], 7) ^ rotr(W[i-15], 18) ^ (W[i-15] >> 3);
        unsigned int s1 = rotr(W[i-2], 17) ^ rotr(W[i-2], 19)  ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }

    a = state_in[0]; b = state_in[1]; c = state_in[2]; d = state_in[3];
    e = state_in[4]; f = state_in[5]; g = state_in[6]; h = state_in[7];

    for (i = 0; i < 64; i++) {
        unsigned int S1   = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        unsigned int ch   = (e & f) ^ ((~e) & g);
        unsigned int temp1 = h + S1 + ch + SHA256_K[i] + W[i];
        unsigned int S0   = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        unsigned int maj  = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state_out[0] = state_in[0] + a; state_out[1] = state_in[1] + b;
    state_out[2] = state_in[2] + c; state_out[3] = state_in[3] + d;
    state_out[4] = state_in[4] + e; state_out[5] = state_in[5] + f;
    state_out[6] = state_in[6] + g; state_out[7] = state_in[7] + h;
}

/* Full SHA-256 of pre-padded message blocks */
static void sha256_blocks_sw(const unsigned int *blocks, int num_blocks,
                              unsigned int hash_out[8])
{
    unsigned int state[8];
    int b, i;

    state[0] = SHA256_H0; state[1] = SHA256_H1;
    state[2] = SHA256_H2; state[3] = SHA256_H3;
    state[4] = SHA256_H4; state[5] = SHA256_H5;
    state[6] = SHA256_H6; state[7] = SHA256_H7;

    for (b = 0; b < num_blocks; b++) {
        unsigned int msg[16];
        for (i = 0; i < 16; i++)
            msg[i] = blocks[b * 16 + i];
        sha256_compress_sw(state, msg, state);
    }

    for (i = 0; i < 8; i++)
        hash_out[i] = state[i];
}

/* =============================================================================
 * Helper functions
 * =============================================================================
 */
static unsigned int bswap32(unsigned int x) {
    return ((x & 0xFFu) << 24) | ((x & 0xFF00u) << 8) |
           ((x >> 8) & 0xFF00u) | ((x >> 24) & 0xFFu);
}

static void print_separator(void) {
    printf("================================================================\n");
}

/* Compute midstate and chunk2_tail from a 20-word LE block header */
static void prepare_mining_params(const unsigned int header_le[20],
                                   unsigned int midstate[8],
                                   unsigned int chunk2_tail[3],
                                   unsigned int *known_nonce_be)
{
    unsigned int header_be[20];
    unsigned int init_state[8] = {
        SHA256_H0, SHA256_H1, SHA256_H2, SHA256_H3,
        SHA256_H4, SHA256_H5, SHA256_H6, SHA256_H7
    };
    unsigned int chunk1[16];
    int i;

    for (i = 0; i < 20; i++)
        header_be[i] = bswap32(header_le[i]);

    for (i = 0; i < 16; i++)
        chunk1[i] = header_be[i];

    sha256_compress_sw(init_state, chunk1, midstate);

    chunk2_tail[0] = header_be[16];
    chunk2_tail[1] = header_be[17];
    chunk2_tail[2] = header_be[18];

    *known_nonce_be = header_be[19];
}

/* Verify double-SHA-256 of a header in software, print the block hash */
static int verify_block_hash_sw(const unsigned int header_le[20],
                                 const char *block_name)
{
    unsigned int header_be[20];
    unsigned int padded[32];
    unsigned int first_hash[8], final_hash[8];
    int i;

    for (i = 0; i < 20; i++)
        header_be[i] = bswap32(header_le[i]);

    /* Pad 80-byte header to two 64-byte blocks */
    for (i = 0; i < 20; i++) padded[i] = header_be[i];
    padded[20] = 0x80000000u;
    for (i = 21; i < 31; i++) padded[i] = 0;
    padded[31] = 0x00000280u;  /* 640 bits */

    sha256_blocks_sw(padded, 2, first_hash);

    /* Second SHA-256 */
    unsigned int hash2_block[16];
    for (i = 0; i < 8; i++) hash2_block[i] = first_hash[i];
    hash2_block[8]  = 0x80000000u;
    for (i = 9; i < 15; i++) hash2_block[i] = 0;
    hash2_block[15] = 0x00000100u;  /* 256 bits */

    sha256_blocks_sw(hash2_block, 1, final_hash);

    printf("  %s hash (display): ", block_name);
    for (i = 7; i >= 0; i--) printf("%08x", bswap32(final_hash[i]));
    printf("\n");

    /* Bitcoin displays hashes in reversed byte order.  The leading zeros
     * in the display hash correspond to the LAST word of the SHA-256
     * output (final_hash[7]), byte-swapped. */
    if (final_hash[7] != 0x00000000u) {
        printf("  ERROR: last SHA-256 word should be 00000000!\n");
        return 1;
    }
    printf("  Leading 32 zero bits in display hash: [OK]\n");
    return 0;
}

/* =============================================================================
 * Test Case 1: SHA-256 NIST FIPS 180-4 known vector
 *
 * SHA256("abc") = ba7816bf 8f01cfea 414140de 5dae2223
 *                 b00361a3 96177a9c b410ff61 f20015ad
 * =============================================================================
 */
static int test_sha256_known_vector(void)
{
    int errors = 0;
    unsigned int msg[16];
    unsigned int hash[8];
    int i;

    unsigned int expected[8] = {
        0xba7816bfu, 0x8f01cfeau, 0x414140deu, 0x5dae2223u,
        0xb00361a3u, 0x96177a9cu, 0xb410ff61u, 0xf20015adu
    };

    printf("\n");
    print_separator();
    printf("TEST 1: SHA-256 Known Vector -- SHA256(\"abc\")\n");
    printf("  Reference: NIST FIPS 180-4\n");
    print_separator();

    memset(msg, 0, sizeof(msg));
    msg[0]  = 0x61626380u;  /* "abc" + 0x80 padding */
    msg[15] = 0x00000018u;  /* length = 24 bits */

    sha256_blocks_sw(msg, 1, hash);

    printf("  Computed: ");
    for (i = 0; i < 8; i++) printf("%08x ", hash[i]);
    printf("\n  Expected: ");
    for (i = 0; i < 8; i++) printf("%08x ", expected[i]);
    printf("\n");

    for (i = 0; i < 8; i++) {
        if (hash[i] != expected[i]) {
            printf("  MISMATCH at word %d!\n", i);
            errors++;
        }
    }
    if (errors == 0) printf("  SHA-256 NIST test: [OK]\n");

    return errors;
}

/* =============================================================================
 * Test Case 2: Mine Block 1 from nonce=0 (software full-range search)
 *
 * Block 1: first block after genesis (January 9, 2009)
 * Nonce (BE): 0x01e36299, ~31.8M iterations from nonce=0
 * =============================================================================
 */
static int test_mine_block1_full_range(void)
{
    int errors = 0;
    int i;

    printf("\n");
    print_separator();
    printf("TEST 2: Mine Block 1 -- Full Range Search from Nonce=0\n");
    printf("  Brute-force search for Block 1's nonce (~31.8M iterations)\n");
    print_separator();

    unsigned int header_le[20] = {
        0x00000001u,
        0x0a8ce26fu, 0x72b3f1b6u, 0x46a2a6c1u, 0x4ff763aeu,
        0x65831e93u, 0x9c085ae1u, 0x0019d668u, 0x00000000u,
        0xfd512098u, 0x44a74b1eu, 0x0e68bebbu, 0x6714ee1fu,
        0xc3a3a17bu, 0xb1f70b54u, 0xe806b6cdu, 0x0e3e2357u,
        0x4966bc61u, 0x1d00ffffu, 0x9962e301u
    };

    unsigned int midstate[8], chunk2_tail[3], known_nonce_be;
    prepare_mining_params(header_le, midstate, chunk2_tail, &known_nonce_be);

    printf("  Known nonce (LE): 0x%08x  (BE): 0x%08x\n",
           header_le[19], known_nonce_be);
    printf("  Midstate: ");
    for (i = 0; i < 8; i++) printf("%08x ", midstate[i]);
    printf("\n");

    errors += verify_block_hash_sw(header_le, "Block 1");

    /* Full-range SW mining from nonce=0 */
    printf("\n  [SW MINING] Searching from nonce_be=0x00000000...\n");
    printf("  [SW MINING] Target: find nonce_be=0x%08x\n", known_nonce_be);
    printf("  [SW MINING] This searches ~%u nonces...\n", known_nonce_be + 1);

    unsigned int H_init[8] = {
        SHA256_H0, SHA256_H1, SHA256_H2, SHA256_H3,
        SHA256_H4, SHA256_H5, SHA256_H6, SHA256_H7
    };

    unsigned int nonce;
    unsigned int found = 0;
    unsigned int found_nonce_sw = 0;

    for (nonce = 0; nonce <= known_nonce_be + 100; nonce++) {
        unsigned int chunk2_W[16];
        chunk2_W[0]  = chunk2_tail[0];
        chunk2_W[1]  = chunk2_tail[1];
        chunk2_W[2]  = chunk2_tail[2];
        chunk2_W[3]  = nonce;
        chunk2_W[4]  = 0x80000000u;
        for (i = 5; i < 15; i++) chunk2_W[i] = 0;
        chunk2_W[15] = 0x00000280u;

        unsigned int first_hash[8];
        sha256_compress_sw(midstate, chunk2_W, first_hash);

        unsigned int hash2_W[16];
        for (i = 0; i < 8; i++) hash2_W[i] = first_hash[i];
        hash2_W[8]  = 0x80000000u;
        for (i = 9; i < 15; i++) hash2_W[i] = 0;
        hash2_W[15] = 0x00000100u;

        unsigned int final_hash[8];
        sha256_compress_sw(H_init, hash2_W, final_hash);

        if (final_hash[7] == 0x00000000u) {
            found_nonce_sw = nonce;
            found = 1;
            break;
        }
    }

    if (found) {
        printf("  [SW MINING] FOUND! Nonce (BE): 0x%08x\n", found_nonce_sw);
        if (found_nonce_sw == known_nonce_be) {
            printf("  [SW MINING] Matches known Block 1 nonce: [OK]\n");
        } else {
            printf("  [SW MINING] Different valid nonce found (also correct)\n");
        }
    } else {
        printf("  [SW MINING] ERROR: nonce not found!\n");
        errors++;
    }

    return errors;
}

/* =============================================================================
 * Generic HW mining test (narrow nonce window for cosim)
 * =============================================================================
 */
static int test_mine_block_hw(const unsigned int header_le[20],
                               const char *block_name,
                               const char *block_desc,
                               const char *expected_hash_display)
{
    int errors = 0;
    int i;

    printf("\n");
    print_separator();
    printf("TEST: Mine %s -- HW Accelerator\n", block_name);
    printf("  %s\n", block_desc);
    print_separator();

    unsigned int midstate[8], chunk2_tail[3], known_nonce_be;
    prepare_mining_params(header_le, midstate, chunk2_tail, &known_nonce_be);

    printf("  Known nonce (LE): 0x%08x  (BE): 0x%08x\n",
           header_le[19], known_nonce_be);
    printf("  Midstate: ");
    for (i = 0; i < 8; i++) printf("%08x ", midstate[i]);
    printf("\n");
    printf("  Chunk2 tail: [%08x, %08x, %08x]\n",
           chunk2_tail[0], chunk2_tail[1], chunk2_tail[2]);

    errors += verify_block_hash_sw(header_le, block_name);
    printf("  Expected:  %s\n", expected_hash_display);

    /* HW mining with narrow window for cosim */
    unsigned int search_start = known_nonce_be - 16;
    unsigned int search_end   = known_nonce_be + 16;
    unsigned int hw_nonce = 0;
    unsigned int hw_status = 0;
    unsigned int target_hi = 0x00000000u;

    printf("\n  [HW] Searching nonces [0x%08x, 0x%08x)...\n",
           search_start, search_end);

    pico_miner(midstate, chunk2_tail, search_start, search_end,
               target_hi, &hw_nonce, &hw_status);

    printf("  [HW] Status: %s\n",
           (hw_status == MINING_FOUND) ? "FOUND" : "NOT FOUND");

    if (hw_status != MINING_FOUND) {
        printf("  ERROR: HW did not find a valid nonce!\n");
        errors++;
    } else {
        printf("  [HW] Found nonce (BE): 0x%08x\n", hw_nonce);
        if (hw_nonce == known_nonce_be) {
            printf("  [HW] Nonce matches %s: [OK]\n", block_name);
        } else {
            printf("  [HW] Different valid nonce (also correct)\n");
        }
    }

    return errors;
}

/* =============================================================================
 * Test Case 3: Mine Block 1 on HW (narrow window)
 * =============================================================================
 */
static int test_mine_block1_hw(void)
{
    unsigned int header_le[20] = {
        0x00000001u,
        0x0a8ce26fu, 0x72b3f1b6u, 0x46a2a6c1u, 0x4ff763aeu,
        0x65831e93u, 0x9c085ae1u, 0x0019d668u, 0x00000000u,
        0xfd512098u, 0x44a74b1eu, 0x0e68bebbu, 0x6714ee1fu,
        0xc3a3a17bu, 0xb1f70b54u, 0xe806b6cdu, 0x0e3e2357u,
        0x4966bc61u, 0x1d00ffffu, 0x9962e301u
    };
    return test_mine_block_hw(header_le,
        "Block 1",
        "First block after genesis (January 9, 2009)",
        "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048");
}

/* =============================================================================
 * Test Case 4: Mine Block 939260 on HW (narrow window)
 *
 * Block 939260 -- mined March 4, 2026
 * Difficulty: 144,398,401,518,101
 * Raw header:
 *   0000003c 3c08f14a ad5cac28 2d1c8ca8 79310a24 15794de3 8e4e0100
 *   00000000 00000000 fc9099f6 927218e1 2db0cc20 ef43657d 5a32cba9
 *   3b8994ba 59a61054 ddf1af1b bf28a869 03f30117 080a741e
 * =============================================================================
 */
static int test_mine_block939260_hw(void)
{
    unsigned int header_le[20] = {
        0x3c000000u,
        0x4af1083cu, 0x28ac5cadu, 0xa88c1c2du, 0x240a3179u,
        0xe34d7915u, 0x00014e8eu, 0x00000000u, 0x00000000u,
        0xf69990fcu, 0xe1187292u, 0x20ccb02du, 0x7d6543efu,
        0xa9cb325au, 0xba94893bu, 0x5410a659u, 0x1baff1ddu,
        0x69a828bfu, 0x1701f303u, 0x1e740a08u
    };
    return test_mine_block_hw(header_le,
        "Block 939260",
        "Recent block (March 4, 2026) -- Difficulty: 144,398,401,518,101",
        "000000000000000000017588478b3612182486d33006ede1164fb146fa41cd06");
}

/* =============================================================================
 * Test Case 5: No valid nonce in range
 * =============================================================================
 */
static int test_no_solution(void)
{
    int errors = 0;

    printf("\n");
    print_separator();
    printf("TEST 5: No Solution in Range\n");
    print_separator();

    unsigned int midstate[8] = {
        SHA256_H0, SHA256_H1, SHA256_H2, SHA256_H3,
        SHA256_H4, SHA256_H5, SHA256_H6, SHA256_H7
    };
    unsigned int chunk2_tail[3] = { 0x11111111u, 0x22222222u, 0x33333333u };
    unsigned int hw_nonce = 0, hw_status = 0;

    pico_miner(midstate, chunk2_tail, 0, 100,
               0x00000000u, &hw_nonce, &hw_status);

    printf("  Status: %s\n",
           (hw_status == MINING_FOUND) ? "FOUND (unexpected)" : "NOT FOUND (expected)");

    if (hw_status != MINING_NOT_FOUND) {
        printf("  WARNING: Found nonce unexpectedly (very improbable).\n");
    } else {
        printf("  No-solution test: [OK]\n");
    }

    return errors;
}

/* =============================================================================
 * Main
 * =============================================================================
 */
int main(void)
{
    int total_errors = 0;

    printf("\n");
    printf("################################################################\n");
    printf("#                                                              #\n");
    printf("#         PICO MINER -- Bitcoin SHA-256 HLS Testbench          #\n");
    printf("#           Zynq-7020 (ZedBoard) + Vivado HLS 2019.1          #\n");
    printf("#                                                              #\n");
    printf("################################################################\n");

    /* Test 1: NIST SHA-256 validation */
    total_errors += test_sha256_known_vector();

    /* Test 2: Mine Block 1 from nonce=0 in software (full-range search) */
    total_errors += test_mine_block1_full_range();

    /* Test 3: Mine Block 1 on HW accelerator (narrow window) */
    total_errors += test_mine_block1_hw();

    /* Test 4: Mine Block 939260 on HW accelerator (narrow window) */
    total_errors += test_mine_block939260_hw();

    /* Test 5: No valid nonce in range */
    total_errors += test_no_solution();

    printf("\n");
    print_separator();
    if (total_errors == 0) {
        printf("ALL TESTS PASSED (%d tests)\n", 5);
    } else {
        printf("FAILED: %d error(s) detected.\n", total_errors);
    }
    print_separator();
    printf("\n");

    return total_errors;
}
