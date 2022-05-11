#include <jansson.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>	
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "network.h"
//#include "sha256-arm.h"
#include "sha-256.h"
#include "block.h"
#include "pl_control.h"


#define POOL_HOSTNAME "stratum.slushpool.com"
#define POOL_PORT 3333
#define USER_NAME "ssmmoo1"
#define USER_PASS "fU5hNeP#$#C3m!w"




#define MAX_CB_SIZE 520

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

//global to fold the block header data
block_header_desc block_header_g;
miner_results results_g;

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
    reset_miner();
    set_miner_parameters(&block_header_g, 39);
    start_miner();
    printf("Started Miner\n");
    while(!miner_is_done())
    {
        usleep(10000);
    }
    get_miner_results(&results_g);
    reset_miner();
  
    printf("Miner complete!\n");
    printf("Block hash found: 0x");
    for(int i = 7; i >= 0; i--) {
        printf("%08X", results_g.hash[i]);
    }
    printf("\nNonce found: %08X\n", results_g.nonce);
    sprintf(good_nonce, "%08x", results_g.nonce);
    pthread_kill((pthread_t) main_thread, SIGUSR1);
}


int main(void)
{
    // Initialize the miner
    assert(initialize_miner() == 0);

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
            print_byte_arr(coinbase_bin, coinbase_bin_size);

            //Calculate double hash of coinbase
            uint8_t temp_hash[32];
            calc_sha_256(temp_hash, coinbase_bin, coinbase_bin_size);
            calc_sha_256(coinbase_hash, temp_hash, 32);
            //sha256_process_arm((uint32_t *)temp_hash, coinbase_bin, coinbase_bin_size);
            //sha256_process_arm((uint32_t *)coinbase_hash, temp_hash, 32);

            printf("Calculated doublehash of coinbase\n");
            print_byte_arr(coinbase_hash, 32);


            //calculate merkle root
            uint8_t merkle_root[32];
            produce_merkle_root(merkle_branches, num_merkle_branches, coinbase_hash, merkle_root);
            printf("Calculated merkle root \n");
            
            //reverse merkle root byte order
            //merkle_root needs to be big endian
            little_2_big(merkle_root, 32);
            print_byte_arr(merkle_root,32);

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

            //Copy intro block header description struct
            memcpy(&(block_header_g.version), version_bytes, 4);
            memcpy(block_header_g.hashPrevBlock, prevhash_bytes, 32);
            memcpy(block_header_g.hashMerkleRoot, merkle_root, 32);
            memcpy(&(block_header_g.timestamp), ntime_bytes, 4);
            memcpy(&(block_header_g.bits), nbits_bytes, 4);

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
