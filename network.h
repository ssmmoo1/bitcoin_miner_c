#include <stdbool.h>
#include <stdio.h>
#include <jansson.h>

//Provide functions to handle networking with the pool


/*
* Resolves the given hostname to an ip address
* ip address is placed in ip_addr pointer 
* returns true if success
* returns false if an error occured
*/
bool host_name_resolution(char* host_name, char* ip_addr);



/*
* Creates a socket connection the given hostname
* Returns the socket file descriptor, should be >0 if successfull
* Returns -1 if there was error. 
*/
int connect_to_pool(char* host_name, int port);


/*
*Should be called after connect_to_pool
*Returns true if login successful
*/
bool login_to_pool(int socket_desc, char* username, char* password);


/*
*Subscribes to block notifications
*Waits for reply and sets the extranonce1 and extranonce2_size 
*/
bool subcribe_block_notifs(int socket_desc, char** extranonce1, int* extranonce2_size);


/*
*Can be called after an RPC to verify there was no error with the RPC
*
*/
bool check_rpc_reply(int socket_desc);

/*
*Receives 1 line from the socket. Deliminated by \n
*Puts line into line pointer
*/
bool receive_line(int socket_desc, char* line, size_t line_size);


/*
*Blocking call that waits for server to request a RPC
*Updates json_reply with RPC data
*Returns true if successfull
*/
bool receive_rpc(int socket_desc, json_t **json_reply);