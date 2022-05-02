// Driver for interfacing with the miner.
// We're going to forego the idea of a kernel
// driver right now, because polling is easy
// and we don't have much to do.

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// The address of the miner that we are interfacing with
#define MINER_ADDRESS 0xA002B000
#define MINER_SIZE (1 << 12)

// The control and data registers of the miner. Each consists
// of one or more 32-bit registers.
#define CONTROL_REG   (0  << 2)
#define STATUS_REG    (1  << 2)
#define TARGET_REG    (4  << 2)
#define VERSION_REG   (7  << 2)
#define HASH_PREV_REG (8  << 2)
#define MERKLE_REG    (16 << 2)
#define TIMESTAMP_REG (32 << 2)
#define BITS_REG      (33 << 2)
#define HASH_OUT_REG  (40 << 2)
#define NONCE_OUT_REG (48 << 2)

// Some helpful constants
#define CONTROL_RESET (0x01)
#define CONTROL_START (0x02)

static int mem_dh = -1;
static uint32_t *miner_h = NULL; 

typedef struct block_header_desc {
    uint32_t version;
    uint32_t hashPrevBlock[8];
    uint32_t hashMerkleRoot[8];
    uint32_t timestamp;
    uint32_t bits;
} block_header_desc;

typedef struct miner_results {
    uint32_t nonce;
    uint32_t hash[8];
} miner_results;

// Constants for frequency setting the PL
static int pl_fs[]  = {300, 250, 187, 150, 100};
static int pl_div[] = {5, 6, 8, 10, 15};

void set_pl_clock_config(int pl_idx, int dh) {
    // Set the clocks according to the configurations listed above.
    printf("Setting PL clock to %d.\n", pl_fs[pl_idx]);

    // Map the necessary registers
    volatile uint8_t * pl_clk_reg = mmap(NULL,
                                         0x1000,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED,
                                         dh,
                                         0xFF5E0000);
    if(pl_clk_reg == MAP_FAILED) {
        perror("Mapping the PL register failed!\n");
    }

    // For the PL clock configuration
    volatile uint32_t * pl0 = (uint32_t*)(pl_clk_reg + 0xC0);

    *pl0 = (1 << 24)
         | (1 << 16)
         | (pl_div[pl_idx] << 8);

    munmap((void*)pl_clk_reg, 0x1000);
}

/**
 * @brief Set the register at the corresponding offset
 * 
 * @param reg_addr The address of the register to write to
 * @param val The value(s) to write to the register
 * @param len The number of (32-bit integer) values to write
 */
static inline void va_set(int reg_addr, uint32_t* val, size_t len) {
    assert(mem_dh != -1);
    assert(miner_h != NULL);
    
    int i = 0;
    while(i < len) {
        miner_h[(reg_addr >> 2) + i] = val[i];
        i++;
    }
}

/**
 * @brief Get the register at the corresponding offset
 * 
 * @param reg_addr The address of the register to read from
 * @param val A pointer to the value(s) to write to
 * @param len The number of (32-bit integer) values to read
 */
static inline void va_get(int reg_addr, uint32_t* val, size_t len) {
    assert(mem_dh != -1);
    assert(miner_h != NULL);
    
    int i = 0;
    while(i < len) {
        val[i] = miner_h[(reg_addr >> 2) + i];
        i++;
    }
}

#define GET_PARAM(reg, var) \
        va_get(reg, (uint32_t*)&var, sizeof(var) / sizeof(uint32_t))

#define SET_PARAM(reg, var) \
        va_set(reg, (uint32_t*)&var, sizeof(var) / sizeof(uint32_t))

/**
 * @brief Set the miner parameters
 * 
 * @param desc The block header, minus the nonce
 * @param target The target number of bits
 */
void set_miner_parameters(block_header_desc *desc, uint32_t target) {

    SET_PARAM(VERSION_REG, desc->version);
    SET_PARAM(HASH_PREV_REG, desc->hashPrevBlock);
    SET_PARAM(MERKLE_REG, desc->hashMerkleRoot);
    SET_PARAM(TIMESTAMP_REG, desc->timestamp);
    SET_PARAM(BITS_REG, desc->bits);
    SET_PARAM(TARGET_REG, target);
}

/**
 * @brief Get the miner results
 * 
 * @param results A pointer to a miner results struct
 */
void get_miner_results(miner_results *results) {
    
    GET_PARAM(HASH_OUT_REG, results->hash);
    GET_PARAM(NONCE_OUT_REG, results->nonce);
}

/**
 * @brief Reset the miner.
 */
void reset_miner() {
    int reset = CONTROL_RESET;
    SET_PARAM(CONTROL_REG, reset);
}

/**
 * @brief Start the miner.
 */
void start_miner() {
    // Reset the miner
    // This is necessary
    reset_miner();

    int start = CONTROL_START;
    SET_PARAM(CONTROL_REG, start);
}

bool miner_is_done() {
    int status;
    GET_PARAM(STATUS_REG, status);

    return !!(status & 0x01);
}

/**
 * @brief Initialize the miner and its driver. Must be called before
 *        any other function in this driver.
 * 
 * @return 0 on success, nonzero otherwise
 */
int initialize_miner() {

    // Initialize our handle to the miner's memory-mapping
    int dh = open("/dev/mem", O_RDWR | O_SYNC);
    if(dh == -1) {
        perror("Unableto open /dev/mem.\n");
        return -1;
    }

    // Set the clock configuration for the PL @ 100MHz
    set_pl_clock_config(4, dh);

    uint32_t *miner = mmap(NULL,
                           MINER_SIZE,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED,
                           dh,
                           MINER_ADDRESS);
    if(miner == MAP_FAILED) {
        perror("Unable to map the miner.\n");
        close(dh);
        return -2;
    }

    mem_dh = dh;
    miner_h = miner;

    reset_miner();

    return 0;
}

/**
 * @brief Free resources associated with the miner.
 */
void deinitialize_miner() {
    // Close our memory handle.
    // This should drop our mem mapping as well.
    close(mem_dh);

    // Reinitialize these variables
    mem_dh = -1;
    miner_h = NULL;
}

// Only for the purposes of testing. Remove this once this
// driver is incorporated into a full suite.
int main() {

    // Initialize the miner
    assert(initialize_miner() == 0);

    // Initialize the block header descriptor to a values that
    // we know (or at least hope) won't take long to find.
    // This should be for block 00000000e5cb7c6c273547b0c9421b01e23310ed83f934b96270f35a4d66f6e3
    block_header_desc desc = {
        .version = 1,
        .hashPrevBlock = {
            // LSB form
            0xb769d3c4,
            0x9bcfc223,
            0x0df03ce3,
            0xeabf1deb,
            0x12cd8c0c,
            0x94f215c4,
            0x9700ff34,
            0x00000000

            // MSB form
            // 0x00000000,
            // 0x9700ff34,
            // 0x94f215c4,
            // 0x12cd8c0c,
            // 0xeabf1deb,
            // 0x0df03ce3,
            // 0x9bcfc223,
            // 0xb769d3c4
        },
        .hashMerkleRoot = {
            // LSB form
            0x0afdc0c9,
            0x3c97b7e7,
            0x3d9efc42,
            0xb667c9dd,
            0x0b5709e3,
            0x54f10f72,
            0x6583c014,
            0x2b9905f0

            // MSB form
            // 0x2b9905f0,
            // 0x6583c014,
            // 0x54f10f72,
            // 0x0b5709e3,
            // 0xb667c9dd,
            // 0x3d9efc42,
            // 0x3c97b7e7,
            // 0x0afdc0c9
        },
        .timestamp = 1231603171,
        .bits = 0x1d00ffff,
    };

    // Set up the miner for the operation
    set_miner_parameters(&desc, 32);

    // Start the miner
    start_miner();

    // Wait for the miner to complete its task
    while(!miner_is_done()) {
        // Do nothing
    }

    // Now that the miner is finished, read back its result
    miner_results results;
    get_miner_results(&results);

    // Print the results in a nice format
    printf("Miner complete!\n");
    printf("Block hash found: 0x");
    for(int i = 7; i >= 0; i--) {
        printf("%08X", results.hash[i]);
    }
    printf("\nNonce found: %08X\n", results.nonce);
}
