#include <stdint.h>
#include <stdbool.h>

#define MAX_EN2_SIZE 20
#define HASH_SIZE 32 //size of a hash in bytes sha-256 is 32 bytes
//Provide functions to create the blocks to hash



/*
*Convert a hex string to a byte array
*/
bool hex_2_byteArr(char* hex_str, uint8_t* bin, int bin_size);


/*
*Returns a hex string with the given size in bytes
*char* must support num_bytes*2 characters
*/
bool produce_extranonce2(uint32_t num_bytes, char* extranonce2);

/*
*Convert little endian byte array to big endian
*Modifies in place
*/
bool little_2_big(uint8_t *bytes, uint32_t size);

/*
*Conver big endian byte array to litte endien
*Modifies in place
*/
bool big_2_little(uint8_t *bytes, uint32_t size);

/*
*Prints out a byte array in hex
*/
void print_byte_arr(uint8_t* bytes, uint32_t size);

/*
*Calculate the merkle root based on an array of branches and the coinbase hash
* merkle_branches - array of hex strings for the merkle branches from the pool
* num_merkle_branches - number of merkle branches in the array of strings
* coinbase_hash - double hash of the coinbase 
* merkle_root - the calculated merkle_root will be put in this array as the return
* returns true of success an false on failure
*
* Calculation coming from here "How to Build Merkle Root" https://braiins.com/stratum-v1/docs
*/
bool produce_merkle_root(char** merkle_branches, uint8_t num_merkle_branches, uint8_t coinbase_hash[HASH_SIZE], uint8_t merkle_root[HASH_SIZE]);