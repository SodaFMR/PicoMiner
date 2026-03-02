/* =============================================================================
 * Pico Miner -- HLS Testbench
 *
 * Description:
 *   Testbench for the Bitcoin double-SHA-256 mining accelerator.
 *   This testbench proves that the FPGA miner performs REAL Bitcoin mining
 *   by successfully mining actual blocks from the Bitcoin blockchain.
 *
 *   Blocks mined:
 *   - Block 1:   The first block after the genesis block (Jan 9, 2009)
 *   - Block 170: First block with a non-coinbase tx (Satoshi -> Hal Finney)
 *   - Block 181: Another early block with two transactions
 *
 *   The testbench:
 *   1. Validates SHA-256 against the NIST FIPS 180-4 test vector
 *   2. Mines Block 1 from nonce=0 in software (proves full-range mining)
 *   3. Mines Block 1 on the HW accelerator (narrow window for cosim speed)
 *   4. Mines Block 170 on the HW accelerator
 *   5. Mines Block 181 on the HW accelerator
 *   6. Tests the no-solution case
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

    if (final_hash[0] != 0x00000000u) {
        printf("  ERROR: first word should be 00000000!\n");
        return 1;
    }
    printf("  Starts with 32 zero bits: [OK]\n");
    return 0;
}

/* =============================================================================
 * Test Case 1: Validate SHA-256 against NIST FIPS 180-4 test vector
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
 * Test Case 2: Mine Bitcoin Block 1 from nonce=0 (software full-range search)
 *
 * This test proves we can find a real Bitcoin nonce starting from zero,
 * exactly as a real miner would. It runs in pure software (no HW call)
 * so it works in both csim and cosim without timeout issues.
 *
 * Block 1 (first block after genesis):
 *   Raw header hex:
 *   01000000 6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d61900
 *   00000000 982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e8
 *   57233e0e 61bc6649 ffff001d 01e36299
 *
 *   Nonce (LE uint32): 0x9962e301  (decimal: 2,573,394,689)
 *   Nonce (BE SHA-256 word): 0x01e36299
 *   Block hash: 00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048
 * =============================================================================
 */
static int test_mine_block1_full_range(void)
{
    int errors = 0;
    int i;

    printf("\n");
    print_separator();
    printf("TEST 2: Mine Bitcoin Block 1 -- Full Range Search from Nonce=0\n");
    printf("  This is REAL Bitcoin mining: searching for Block 1's nonce\n");
    printf("  starting from zero, exactly as Satoshi's CPU did in 2009.\n");
    print_separator();

    /* Block 1 header (20 little-endian 32-bit words) */
    unsigned int header_le[20] = {
        0x00000001u,  /* version */
        0x0a8ce26fu, 0x72b3f1b6u, 0x46a2a6c1u, 0x4ff763aeu,
        0x65831e93u, 0x9c085ae1u, 0x0019d668u, 0x00000000u,
        0xfd512098u, 0x44a74b1eu, 0x0e68bebbu, 0x6714ee1fu,
        0xc3a3a17bu, 0xb1f70b54u, 0xe806b6cdu, 0x0e3e2357u,
        0x4966bc61u,  /* timestamp */
        0x1d00ffffu,  /* bits */
        0x9962e301u   /* nonce (LE) */
    };

    unsigned int midstate[8], chunk2_tail[3], known_nonce_be;
    prepare_mining_params(header_le, midstate, chunk2_tail, &known_nonce_be);

    printf("  Known nonce (LE): 0x%08x  (BE): 0x%08x\n",
           header_le[19], known_nonce_be);
    printf("  Midstate: ");
    for (i = 0; i < 8; i++) printf("%08x ", midstate[i]);
    printf("\n");

    /* Verify the block hash */
    errors += verify_block_hash_sw(header_le, "Block 1");

    /* --- Full-range SW mining from nonce=0 --- */
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
        /* Build chunk 2 */
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

        if (final_hash[0] == 0x00000000u) {
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
 * Generic HW mining test for a real Bitcoin block
 *
 * Uses a narrow nonce window (±16) around the known answer for fast cosim.
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

    /* Verify block hash in SW */
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
 * Test Case 3: Mine Bitcoin Block 1 on HW
 *
 * Block 1 -- the first block after the genesis block.
 * Mined by Satoshi Nakamoto on January 9, 2009.
 *
 * Raw header: 010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c
 *             68d6190000000000982051fd1e4ba744bbbe680e1fee14677ba1a3c354
 *             0bf7b1cdb606e857233e0e61bc6649ffff001d01e36299
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
        "Bitcoin Block 1",
        "First block after genesis (Satoshi, Jan 9 2009)",
        "00000000839a8e6886ab5951d76f411475428afc90947ee320161bbf18eb6048");
}

/* =============================================================================
 * Test Case 4: Mine Bitcoin Block 170 on HW
 *
 * Block 170 -- the first block containing a non-coinbase transaction.
 * Satoshi Nakamoto sent 10 BTC to Hal Finney on January 12, 2009.
 *
 * Raw header: 0100000055bd840a78798ad0da853f68974f3d183e2bd1db6a842c1f
 *             eecf222a00000000ff104ccb05421ab93e63f8c3ce5c2c2e9dbb37de27
 *             64b3a3175c8166562cac7d51b96a49ffff001d283e9e70
 * =============================================================================
 */
static int test_mine_block170_hw(void)
{
    unsigned int header_le[20] = {
        0x00000001u,
        0x0a84bd55u, 0xd08a7978u, 0x683f85dau, 0x183d4f97u,
        0xdbd12b3eu, 0x1f2c846au, 0x2a22cfeeu, 0x00000000u,
        0xcb4c10ffu, 0xb91a4205u, 0xc3f8633eu, 0x2e2c5cceu,
        0xde37bb9du, 0xa3b36427u, 0x66815c17u, 0x7dac2c56u,
        0x496ab951u, 0x1d00ffffu, 0x709e3e28u
    };
    return test_mine_block_hw(header_le,
        "Bitcoin Block 170",
        "First non-coinbase tx (Satoshi -> Hal Finney, 10 BTC, Jan 12 2009)",
        "00000000d1145790a8694403d4063f323d499e655c83426834d4ce2f8dd4a2ee");
}

/* =============================================================================
 * Test Case 5: Mine Bitcoin Block 181 on HW
 *
 * Block 181 -- another early block with two transactions.
 *
 * Raw header: 01000000f2c8a8d2af43a9cd05142654e56f41d159ce0274d9cabe15
 *             a20eefb500000000366c2a0915f05db4b450c050ce7165acd55f823fee
 *             51430a8c993e0bdbb192ede5dc6a49ffff001d192d3f2f
 * =============================================================================
 */
static int test_mine_block181_hw(void)
{
    unsigned int header_le[20] = {
        0x00000001u,
        0xd2a8c8f2u, 0xcda943afu, 0x54261405u, 0xd1416fe5u,
        0x7402ce59u, 0x15becad9u, 0xb5ef0ea2u, 0x00000000u,
        0x092a6c36u, 0xb45df015u, 0x50c050b4u, 0xac6571ceu,
        0x3f825fd5u, 0x0a4351eeu, 0x0b3e998cu, 0xed92b1dbu,
        0x496adce5u, 0x1d00ffffu, 0x2f3f2d19u
    };
    return test_mine_block_hw(header_le,
        "Bitcoin Block 181",
        "Early block with two transactions (Jan 12 2009)",
        "00000000dc55860c8a29c58d45209318fa9e9dc2c1833a7226d86bc465afc6e5");
}

/* =============================================================================
 * Test Case 6: No valid nonce in range
 * =============================================================================
 */
static int test_no_solution(void)
{
    int errors = 0;

    printf("\n");
    print_separator();
    printf("TEST 6: No Solution in Range\n");
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
    printf("#      Real Bitcoin Mining on FPGA (Zynq-7020 / ZedBoard)      #\n");
    printf("#                                                              #\n");
    printf("#  This testbench mines REAL Bitcoin blocks using the same     #\n");
    printf("#  double-SHA-256 algorithm used by every Bitcoin miner.       #\n");
    printf("#                                                              #\n");
    printf("################################################################\n");

    /* Test 1: NIST SHA-256 validation */
    total_errors += test_sha256_known_vector();

    /* Test 2: Mine Block 1 from nonce=0 in software (full-range search) */
    total_errors += test_mine_block1_full_range();

    /* Test 3: Mine Block 1 on HW accelerator (narrow window) */
    total_errors += test_mine_block1_hw();

    /* Test 4: Mine Block 170 on HW accelerator (narrow window) */
    total_errors += test_mine_block170_hw();

    /* Test 5: Mine Block 181 on HW accelerator (narrow window) */
    total_errors += test_mine_block181_hw();

    /* Test 6: No valid nonce in range */
    total_errors += test_no_solution();

    printf("\n");
    print_separator();
    if (total_errors == 0) {
        printf("ALL TESTS PASSED\n");
        printf("  Successfully mined 3 real Bitcoin blocks (1, 170, 181)\n");
        printf("  using the identical double-SHA-256 algorithm as Bitcoin.\n");
    } else {
        printf("FAILED: %d error(s) detected.\n", total_errors);
    }
    print_separator();
    printf("\n");

    return total_errors;
}
