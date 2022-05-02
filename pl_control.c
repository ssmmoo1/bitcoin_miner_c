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
#include "pl_control.h"

static int mem_dh = -1;
static uint32_t *miner_h = NULL; 

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