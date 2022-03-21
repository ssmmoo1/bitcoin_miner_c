#include "block.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/*
*Convert a hex string to a byte array
*/
bool hex_2_byteArr(char* hex_str, uint8_t* bin, int bin_size)
{
    char* hex_pos = hex_str;

    for (int i = 0; i < bin_size; i++) {
        sscanf(hex_pos, "%2hhx", &bin[i]);
        hex_pos += 2;
    }
    return true;
}


/*
*Returns a hex string with the given size in bytes
*char* must support num_bytes*2 characters
*/
bool produce_extranonce2(uint32_t num_bytes, char* extranonce2)
{
    static int current_val = 0;
    memset(extranonce2, 0, MAX_EN2_SIZE);

    char format[6] = "%0";
    char num_bytes_s[3];
    sprintf(num_bytes_s, "%d", num_bytes*2);
    strcat(format, num_bytes_s);
    strcat(format, "x");

    sprintf(extranonce2, format, current_val);
    printf("Generating extranonce2:%s\n", extranonce2);

    current_val +=1;
}




/*
*Convert little endian byte array to big endian
*Modifies in place
*/
bool little_2_big(uint8_t *bytes, uint32_t size)
{
    uint8_t *temp_arr = malloc(size);
    memcpy(temp_arr, bytes, size);

    int temp_i = 0;
    for(int i = size-1; i >=0; i--)
    {
        bytes[i] = temp_arr[temp_i];
        temp_i++;
    }
    free(temp_arr);
    return true;
}

/*
*Conver big endian byte array to litte endien
*Modifies in place
*/
bool big_2_little(uint8_t *bytes, uint32_t size)
{
    uint8_t *temp_arr = malloc(size);
    memcpy(temp_arr, bytes, size);

    int temp_i = size-1;
    for(int i = 0; i<size; i++)
    {
        bytes[i] = temp_arr[temp_i];
        temp_i--;
    }
    free(temp_arr);
    return true;
}

/*
*Prints out a byte array in hex
*/
void print_byte_arr(uint8_t* bytes, uint32_t size)
{
    printf("Printing byte array:");
    for(int i = 0; i < size; i++)
    {
        printf("%02x ", bytes[i]);
    }
    printf("\n");

}