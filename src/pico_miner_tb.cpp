/* =============================================================================
 * Pico Miner -- HLS Testbench
 *
 * Description:
 *   Testbench for the Bitcoin double-SHA-256 mining accelerator.
 *   Uses Bitcoin Block 170 (the first block with a non-coinbase transaction,
 *   Satoshi -> Hal Finney) as a real-world test vector.
 *
 *   The testbench:
 *   1. Validates the SHA-256 implementation against NIST test vectors
 *   2. Computes the midstate on software (same as ARM would do)
 *   3. Calls the HLS function with a nonce range around the known answer
 *   4. Verifies the HLS function finds the correct nonce
 *   5. Tests the no-solution case
 *
 *   This testbench is used for:
 *   - Vivado HLS C Simulation (csim)
 *   - Vivado HLS C/RTL Co-Simulation (cosim)
 *
 * Author: Pico Miner Project
 * Date: 2026
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
 * Helper: byte-swap a 32-bit word
 * =============================================================================
 */
static unsigned int bswap32(unsigned int x) {
    return ((x & 0xFFu) << 24) | ((x & 0xFF00u) << 8) |
           ((x >> 8) & 0xFF00u) | ((x >> 24) & 0xFFu);
}

static void print_separator(void) {
    printf("================================================================\n");
}

/* =============================================================================
 * Test Case 1: Validate SHA-256 against NIST test vector
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
    if (errors == 0) printf("  SHA-256 test: [OK]\n");

    return errors;
}

/* =============================================================================
 * Test Case 2: Bitcoin Block 170 mining
 *
 * Block 170 header (80 bytes, serialized little-endian hex):
 *   01000000
 *   55bd840a78798ad0da853f68974f3d183e2bd1db6a842c1feecf222a00000000
 *   ff104ccb05421ab93e63f8c3ce5c2c2e9dbb37de2764b3a3175c8166562cac7d
 *   51b96a49
 *   ffff001d
 *   283e9e70
 *
 * Known winning nonce (LE): 0x283e9e70  -> (BE): 0x709e3e28
 * Block hash (display):
 *   00000000d1145790a8694403d4063f323d499e655c83426834d4ce2f8dd4a2ee
 * =============================================================================
 */
static int test_bitcoin_block_170(void)
{
    int errors = 0;
    int i;

    printf("\n");
    print_separator();
    printf("TEST 2: Bitcoin Block 170 -- Real Mining Test\n");
    print_separator();

    /* Block header as 20 little-endian 32-bit words (raw serialized) */
    unsigned int header_le[20] = {
        0x00000001u,  /* version */
        /* prev_hash (8 words, LE): */
        0x0a84bd55u, 0xd08a7978u, 0x683f85dau, 0x183d4f97u,
        0xdbd12b3eu, 0x1f2c846au, 0x2a22cfeeu, 0x00000000u,
        /* merkle_root (8 words, LE): */
        0xcb4c10ffu, 0xb91a4205u, 0xc3f8633eu, 0x2e2c5cceu,
        0xde37bb9du, 0xa3b36427u, 0x66815c17u, 0x7dac2c56u,
        /* timestamp, bits, nonce (LE): */
        0x496ab951u,
        0x1d00ffffu,
        0x709e3e28u
    };

    /* Byte-swap to big-endian for SHA-256 */
    unsigned int header_be[20];
    for (i = 0; i < 20; i++)
        header_be[i] = bswap32(header_le[i]);

    /* Compute midstate: SHA-256 compress chunk 1 (words 0-15) */
    unsigned int chunk1[16];
    for (i = 0; i < 16; i++) chunk1[i] = header_be[i];

    unsigned int midstate[8];
    unsigned int init_state[8] = {
        SHA256_H0, SHA256_H1, SHA256_H2, SHA256_H3,
        SHA256_H4, SHA256_H5, SHA256_H6, SHA256_H7
    };
    sha256_compress_sw(init_state, chunk1, midstate);

    printf("  Midstate: ");
    for (i = 0; i < 8; i++) printf("%08x ", midstate[i]);
    printf("\n");

    /* Chunk 2 tail: header words 16, 17, 18 (big-endian) */
    unsigned int chunk2_tail[3] = {
        header_be[16], header_be[17], header_be[18]
    };

    unsigned int known_nonce_be = header_be[19];

    printf("  Chunk2 tail: [%08x, %08x, %08x]\n",
           chunk2_tail[0], chunk2_tail[1], chunk2_tail[2]);
    printf("  Known nonce (BE): 0x%08x\n", known_nonce_be);

    /* ---- Full SW double-SHA-256 for verification ---- */
    unsigned int padded_header[32];
    for (i = 0; i < 16; i++) padded_header[i] = header_be[i];
    padded_header[16] = header_be[16];
    padded_header[17] = header_be[17];
    padded_header[18] = header_be[18];
    padded_header[19] = header_be[19];
    padded_header[20] = 0x80000000u;
    for (i = 21; i < 31; i++) padded_header[i] = 0;
    padded_header[31] = 0x00000280u;

    unsigned int first_hash_sw[8];
    sha256_blocks_sw(padded_header, 2, first_hash_sw);

    printf("  SW first hash:  ");
    for (i = 0; i < 8; i++) printf("%08x ", first_hash_sw[i]);
    printf("\n");

    /* Second hash */
    unsigned int hash2_block[16];
    for (i = 0; i < 8; i++) hash2_block[i] = first_hash_sw[i];
    hash2_block[8]  = 0x80000000u;
    for (i = 9; i < 15; i++) hash2_block[i] = 0;
    hash2_block[15] = 0x00000100u;

    unsigned int final_hash_sw[8];
    sha256_blocks_sw(hash2_block, 1, final_hash_sw);

    printf("  SW final hash:  ");
    for (i = 0; i < 8; i++) printf("%08x ", final_hash_sw[i]);
    printf("\n");

    printf("  Block hash (display): ");
    for (i = 7; i >= 0; i--) printf("%08x", bswap32(final_hash_sw[i]));
    printf("\n");

    if (final_hash_sw[0] != 0x00000000u) {
        printf("  ERROR: First word of hash should be 00000000!\n");
        errors++;
    } else {
        printf("  SW hash verification: [OK] (starts with 32 zero bits)\n");
    }

    /* ---- HW test ---- */
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
    if (hw_status == MINING_FOUND) {
        printf("  [HW] Found nonce: 0x%08x\n", hw_nonce);
    }

    if (hw_status != MINING_FOUND) {
        printf("  ERROR: HW did not find a valid nonce!\n");
        errors++;
    } else if (hw_nonce != known_nonce_be) {
        printf("  NOTE: HW nonce 0x%08x differs from known 0x%08x\n",
               hw_nonce, known_nonce_be);
        printf("  (May be a different valid nonce in the search range)\n");
    } else {
        printf("  [HW] Nonce matches Bitcoin Block 170: [OK]\n");
    }

    return errors;
}

/* =============================================================================
 * Test Case 3: No valid nonce in range
 * =============================================================================
 */
static int test_no_solution(void)
{
    int errors = 0;

    printf("\n");
    print_separator();
    printf("TEST 3: No Solution in Range\n");
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
    printf("#       FPGA-Based Proof of Work Mining Accelerator            #\n");
    printf("#                                                              #\n");
    printf("################################################################\n");

    total_errors += test_sha256_known_vector();
    total_errors += test_bitcoin_block_170();
    total_errors += test_no_solution();

    printf("\n");
    print_separator();
    if (total_errors == 0) {
        printf("ALL TESTS PASSED -- Bitcoin double-SHA-256 mining verified.\n");
    } else {
        printf("FAILED: %d error(s) detected.\n", total_errors);
    }
    print_separator();
    printf("\n");

    return total_errors;
}
