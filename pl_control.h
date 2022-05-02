#include <stdint.h>
#include <stdbool.h>

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



void set_pl_clock_config(int pl_idx, int dh);



/**
 * @brief Set the register at the corresponding offset
 * 
 * @param reg_addr The address of the register to write to
 * @param val The value(s) to write to the register
 * @param len The number of (32-bit integer) values to write
 */
static inline void va_set(int reg_addr, uint32_t* val, size_t len);



/**
 * @brief Get the register at the corresponding offset
 * 
 * @param reg_addr The address of the register to read from
 * @param val A pointer to the value(s) to write to
 * @param len The number of (32-bit integer) values to read
 */
static inline void va_get(int reg_addr, uint32_t* val, size_t len);


#define GET_PARAM(reg, var) \
        va_get(reg, (uint32_t*)&var, sizeof(var) / sizeof(uint32_t))

#define SET_PARAM(reg, var) \
        va_set(reg, (uint32_t*)&var, sizeof(var) / sizeof(uint32_t))



/**
 * @brief Free resources associated with the miner.
 */
void deinitialize_miner(void);

/**
 * @brief Initialize the miner and its driver. Must be called before
 *        any other function in this driver.
 * 
 * @return 0 on success, nonzero otherwise
 */
int initialize_miner(void);

bool miner_is_done();

/**
 * @brief Start the miner.
 */
void start_miner();

/**
 * @brief Reset the miner.
 */
void reset_miner();

/**
 * @brief Get the miner results
 * 
 * @param results A pointer to a miner results struct
 */
void get_miner_results(miner_results *results);

/**
 * @brief Set the miner parameters
 * 
 * @param desc The block header, minus the nonce
 * @param target The target number of bits
 */
void set_miner_parameters(block_header_desc *desc, uint32_t target);