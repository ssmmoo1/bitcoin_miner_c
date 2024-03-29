#include <jansson.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>	
#include <arpa/inet.h>

#include <pthread.h>
#include <signal.h>

#include "network.h"
#include "sha-256.h"
#include "block.h"


#define POOL_HOSTNAME "stratum.slushpool.com"
#define POOL_PORT 3333
#define USER_NAME "user"
#define USER_PASS "pass"


/*
#define POOL_HOSTNAME "prohashing.com"
#define POOL_PORT 3335
#define USER_NAME "rawep"
#define USER_PASS "a=sha-256,n=test,o=test"
*/

#define MAX_CB_SIZE 550

//block variables to compute hashes
static uint32_t block_difficulty = 0;

//const char* values are filled in by json unpack
char* job_id;
char* prevhash;
char* coinb1;
char* coinb2;
char* extranonce1;

char coinbase_hex[MAX_CB_SIZE];
uint8_t *coinbase_bin = NULL;
uint32_t coinbase_bin_size = 0;

uint8_t coinbase_hash[32];

char extranonce2[MAX_EN2_SIZE];
uint32_t extranonce2_size;
char good_nonce[9];
char* version;
char* nbits;
char* ntime;

char** merkle_branches;
uint8_t num_merkle_branches = 0;

uint8_t block_header[80];
uint8_t difficulty_target[32] = {0,0,0,0,0,127,255,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//uint8_t difficulty_target[32] = {0,0,0,255,255,127,255,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int socket_desc_g;


void block_found_handler(int signal)
{
    printf("Good nonce:%s\n", good_nonce);
    if(submit_block(socket_desc_g, USER_NAME, job_id, extranonce2, ntime, good_nonce) == false)
    {
        exit(1);
    }
    
}

bool check_hash(uint8_t hash[32], uint8_t target[32])
{
    int zeros = 0;
    for(int i = 31; i >=0; i--)
    {
        if(hash[i] < target[i])
        {
            return true;
        }
        else if(hash[i]-target[i] > 0)
        {
            return false;
        }
    }
    return false;
}


bool compute_target(uint32_t difficulty, uint8_t target[32])
{
    uint8_t base_target[32];
    memset(base_target, 0xFF, 32);
    

    //calc log2 of difficult
    int shifts = 0;
    while (difficulty >>= 1) ++shifts;
    printf("Number of shifts %d\n", shifts);

    
    int zeroes = 32 + shifts;
    for(int i = 0; i < zeroes / 8; i++)
    {
        base_target[i] = 0;
    }
    uint8_t leftover = 0xFF;
    leftover >>= zeroes % 8;
    base_target[zeroes/8] = leftover;

    memcpy(target, base_target, 32);
    big_2_little(target, 32);
    printf("Computed target difficulty:");
    print_byte_arr(target, 32);
    return true;
}



void* mining_thread(void* main_thread)
{
    printf("Mining thread created\n");
    uint8_t block_header_local[80];
    uint8_t block_hash[32];
    uint8_t temp_hash[32];
    memcpy(block_header_local, block_header, 80);

    for(uint32_t nonce=0; nonce<= 0xFFFFFFFF; nonce++)
    {
        block_header_local[28] = (nonce & 0xFF000000) >> 24;
        block_header_local[29] = (nonce & 0x00FF0000) >> 16;
        block_header_local[30] = (nonce & 0x0000FF00) >> 8;
        block_header_local[31] =  nonce & 0x000000FF;

        if(nonce % 1000000 == 0)
        {
            printf("On nonce %d\n", nonce);

            print_byte_arr(block_hash, 32);
        }

        calc_sha_256(temp_hash, block_header_local, 80);
        calc_sha_256(block_hash, temp_hash, 32);
        
        if(check_hash(block_hash, difficulty_target) == true)
        {
            sprintf(good_nonce, "%08x", nonce);
            printf("Found a hash\n");
            print_byte_arr(block_hash, 32);
            pthread_kill((pthread_t) main_thread, SIGUSR1);
            break;
        }
    }
}


int main(void)
{
    //Register handler for finding a block
    signal(SIGUSR1, block_found_handler);
    pthread_t mining_thread_id = (pthread_t) NULL; //setup pthread for later
    
    //json decode vars
    json_t* json_reply;
    json_t* get_value;

    //Create socket connection to pool
    int socket_desc = connect_to_pool(POOL_HOSTNAME, POOL_PORT);
    if(socket_desc < 0)
    { 
        return 1;
    }
    socket_desc_g = socket_desc;
   
    //Send login credentials to pool
    if(login_to_pool(socket_desc, USER_NAME, USER_PASS) == false)
    {
        return 1;
    }
   
    //Attempt to subscribe to block notifications
    if(subcribe_block_notifs(socket_desc, &extranonce1, &extranonce2_size) == false)
    {
        return 1;
    }
    
    //now that we have the extranonce2_size, generate the extranonce2
    produce_extranonce2(extranonce2_size, extranonce2);

    //Loop and receive RPC calls
     
    const char *rpc_method;
    while(1)
    {
        //Wait for a remote procedure call then handle the call
        receive_rpc(socket_desc, &json_reply);
        json_unpack(json_reply, "{s:s}", "method", &rpc_method);
        printf("Received RPC with method: %s\n", rpc_method);

        //handle RPC calls

        //Update mining difficult
        if(strcmp(rpc_method, "mining.set_difficulty") == 0)
        {
            json_unpack(json_reply, "{s:[i]}", "params", &block_difficulty);
            printf("Block difficult set to %d\n", block_difficulty);
            compute_target(block_difficulty, difficulty_target); //update target difficulty
        }

        //Update block being mined
        else if(strcmp(rpc_method, "mining.notify") == 0)
        {
            printf("Updating block data\n");

            json_t *merkle_json;
            json_unpack(json_reply, "{s:[s,s,s,s,o,s,s,s]}", "params", &job_id, &prevhash, &coinb1, &coinb2, &merkle_json, &version, &nbits, &ntime);
            
            //read in all parts of the merkle branch
            num_merkle_branches = json_array_size(merkle_json);
            printf("Number of merkle branches: %d\n", num_merkle_branches);
            printf("Unpacking merkle roots\n");

             if(merkle_branches != NULL)
            {
                free(merkle_branches);
            }
            merkle_branches = malloc(num_merkle_branches * sizeof(char *));

            for(int i = 0; i<num_merkle_branches; i++)
            {
                json_unpack(json_array_get(merkle_json, i), "s", merkle_branches + i );
                printf("\tRoot %d:%s\n", i, merkle_branches[i]);
            }
            //should have all block data now
            printf("Block Data\n\tjob_id:%s\n\tprevhash:%s\n\tcoinb1:%s\n\tcoinb2:%s\n", job_id, prevhash, coinb1, coinb2);


            //Generate coinbase coinb1 + extranonce1 + extranonce2 + coinb2
            memset(coinbase_hex, 0, MAX_CB_SIZE);
            sprintf(coinbase_hex, "%s%s%s%s", coinb1, extranonce1, extranonce2, coinb2);
            printf("Generated hex coinbase:%s\n", coinbase_hex);

            //convert coinbase to byte array
            if(coinbase_bin != NULL)
            {
                free(coinbase_bin);
            }
            coinbase_bin_size = sizeof(coinbase_hex) / 2;
            coinbase_bin = malloc(coinbase_bin_size);
            memset(coinbase_bin, 0, coinbase_bin_size);

            hex_2_byteArr(coinbase_hex, coinbase_bin, coinbase_bin_size);
            printf("Converted coinbase to byte array \n");

            //Calculate double hash of coinbase
            uint8_t temp_hash[32];
            calc_sha_256(temp_hash, coinbase_bin, coinbase_bin_size);
            calc_sha_256(coinbase_hash, temp_hash, 32);

            printf("Calculated doublehash of coinbase\n");


            //calculate merkle root
            uint8_t merkle_root[32];
            produce_merkle_root(merkle_branches, num_merkle_branches, coinbase_hash, merkle_root);
            printf("Calculated merkle root \n");
            
            //reverse merkle root byte order
            //merkle_root needs to be big endian
            little_2_big(merkle_root, 32);

            //Build block header
            //version + previous hash + merkle root + timestamp + nbits + nonce
            uint8_t version_bytes[4];
            uint8_t prevhash_bytes[32];
            uint8_t ntime_bytes[4];
            uint8_t nbits_bytes[4];

            //convert all fields to byteArrays
            hex_2_byteArr(version, version_bytes, 4);
            hex_2_byteArr(prevhash, prevhash_bytes, 32);
            hex_2_byteArr(ntime, ntime_bytes, 4);
            hex_2_byteArr(nbits, nbits_bytes, 4);

            printf("Converted all headers to bytes\n");

            //Copy all fields in the correct order to form the full block header (not including nonce)
            uint8_t *block_header_pointer = block_header;
            //block header = version + prevhash + merkle_root + ntime + nbits + nonce
            memcpy(block_header_pointer, version_bytes, 4);
            block_header_pointer +=4;

            memcpy(block_header_pointer, prevhash_bytes, 32);
            block_header_pointer += 32;
       

            memcpy(block_header_pointer, merkle_root, 32);
            block_header_pointer +=32;
        

            memcpy(block_header_pointer, ntime_bytes, 4);
            block_header_pointer+=4;
     

            memcpy(block_header_pointer, nbits_bytes, 4);
            block_header_pointer+=4;

            printf("Created block header up to nonce\n"); //nonce is added in by the mining thread

            //On block update, cancel old mining thread and start new one
            if(mining_thread_id != (pthread_t) NULL)
            {
                pthread_cancel(mining_thread_id);
                pthread_join(mining_thread_id, NULL);
                printf("Killed old mining thread\n");
            }
            pthread_create(&mining_thread_id, NULL, mining_thread, (void *) pthread_self());

        }

        else
        {
            printf("Invalid RPC\n");
        }


    }


}
