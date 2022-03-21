#include <stdint.h>
#include <stdbool.h>

#define MAX_EN2_SIZE 20
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