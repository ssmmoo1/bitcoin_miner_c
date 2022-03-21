#include <stdbool.h>
#include <netdb.h>	
#include<arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <jansson.h>

#include "network.h"

#define RECV_SIZE 10000

/*
* Resolves the given hostname to an ip address
* ip address is placed in ip_addr pointer 
* returns true if success
* returns false if an error occured
*/
bool host_name_resolution(char* host_name, char* ip_addr)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;

    if((he=gethostbyname(host_name)) == NULL)
    {
        printf("Error resolving hostname\n");
        return false;
    }

    addr_list = (struct in_addr **) he->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++)
    {
        strcpy(ip_addr, inet_ntoa(*addr_list[i]));
    }

    printf("%s resolved to : %s\n", host_name, ip_addr);
    return true;
}

/*
* Creates a socket connection the given hostname
* Returns the socket file descriptor, should be >0 if successfull
* Returns -1 if there was error. 
*/
int connect_to_pool(char* host_name, int port)
{
    //Attemp to Create Socket
    int socket_desc;
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc == -1)
    {
        printf("Could not create socket\n");
        return -1;
    }

    //Attempt to resolve hostname
    char ip[100];
    if(host_name_resolution(host_name, ip) == false)
    {
        return -1;
    }

    //Attempt to bind socket
    struct sockaddr_in server = {.sin_family=AF_INET, .sin_port=htons(port), .sin_addr.s_addr=inet_addr(ip)};
    if(connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("Could not bind socket\n");
        return -1;
    }
    else
    {
        printf("Connected to socket successfully\n");
        return socket_desc;
    }
}



/*
*Should be called after connect_to_pool
*Returns true if login successful
*/
bool login_to_pool(int socket_desc, char* username, char* password)
{
    char message[200];
    sprintf(message, "{\"params\": [\"%s\", \"%s\"], \"id\": 1, \"method\": \"mining.authorize\"}\n", username, password);
    if(send(socket_desc, message, strlen(message), 0) < 0)
    {
        printf("Failed to send message over socket\n");
        return false;
    }
    else
    {
        printf("Successfully sent login data over socket\n");
    }

    if(check_rpc_reply(socket_desc) == true)
    {
        printf("Login was successful\n");
        return true;
    }
    else
    {
        printf("login RPC failed \n");
        return false;
    }

}


/*
*Subscribes to block notifications
*Waits for reply and sets the extranonce1 and extranonce2_size 
*/
bool subcribe_block_notifs(int socket_desc, char** extranonce1, int* extranonce2_size)
{ 
    json_error_t j_err;
    json_t *json_reply;
    json_t *throw_away;


    char message[200] = "{\"id\": 2, \"method\": \"mining.subscribe\", \"params\": []}\n";
    char reply[RECV_SIZE];
    memset(reply, 0, RECV_SIZE);

    if(send(socket_desc, message, strlen(message), 0) < 0)
    {
        printf("Failed to send message over socket\n");
        return false;
    }
    else
    {
        printf("Successfully sent data over socket\n");
    }

    receive_line(socket_desc, reply, RECV_SIZE);
    
    json_reply = json_loads(reply, 0, &j_err);
    json_unpack(json_reply, "{s:[o,s,i]}", "result", &throw_away, extranonce1, extranonce2_size);
    printf("Received line: %s\n", reply);
    printf("Updated extranonce1:%s and extranonce2_size:%d\n", *extranonce1, *extranonce2_size);
    return true;
}


/*
*Blocking call that waits for server to request a RPC
*Updates json_reply with RPC data
*Returns true if successfull
*/
bool receive_rpc(int socket_desc, json_t **json_reply)
{
    json_error_t j_err;
    char reply[RECV_SIZE];
    memset(reply, 0, RECV_SIZE);
    //Get a line
    if(receive_line(socket_desc, reply, RECV_SIZE))
    {
        printf("Received line: %s\n", reply);
    }
    else
    {
        printf("Error receiving line\n");
        return false;
    }

    //Attempt to decode into json
    *json_reply = json_loads(reply, 0, &j_err);
    if(json_reply == NULL)
    {
        printf("Error decoding json");
        return false;
    }
    else
    {
        printf("Successfully Decoded Json\n");
        return true;
    }

}


/*
*Should be called after sending an RPC request
*This will wait for the request response and check that the error in the reply is null 
*Returns true if successfull, returns false if the RPC had an error. 
*/
bool check_rpc_reply(int socket_desc)
{
    char reply[RECV_SIZE];
    //Attempt to receive login acknowledgement
    if(receive_line(socket_desc, reply, RECV_SIZE))
    {
        printf("Received line: %s\n", reply);
    }
    else
    {
        printf("Error receiving line\n");
        return false;
    }

    //Attempt to decode into json
    json_error_t j_err;
    json_t *json_reply = json_loads(reply, 0, &j_err);
    if(json_reply == NULL)
    {
        printf("Error decoding json\n");
        return false;
    }
    else
    {
        printf("Successfully Decoded Json\n");
    }

    //Verify RPC was successfull
    json_t *get_value = json_object_get(json_reply, "error");
    if(json_is_null(get_value))
    {
        printf("RPC had no errors\n");
        return true;
    }
    else
    {
        printf("RPC responded with an error\n");
        return false;
    }
}

/*
*Receives 1 line from the socket. Deliminated by \n
*Puts line into line pointer
*Returns true of success
*/
bool receive_line(int socket_desc, char* line, size_t line_size)
{
    memset(line, 0, line_size);

    uint32_t line_index = 0;

    char buffer;
    while(buffer != '\n')
    {
        if( recv(socket_desc, &buffer, 1, 0) < 0)
        {
            printf("Error receiving line from socket, recv call failed \n");
            return false;
        }
        else
        {
            line[line_index] = buffer;
            line_index+=1;
            if(line_index >= line_size-1)
            {
                printf("Error receiving line from socket, buffer not large enough \n");
                return false;
            }
        }  
    }
    return true;
}
