#ifndef UTILITIES_H
#define UTILITIES_H

#include <sys/socket.h>
#include <openssl/aes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>

typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;

typedef struct
{
	int client_fd;
	int target_fd;
} thread_arg;

typedef struct
{
    unsigned char ivec[AES_BLOCK_SIZE];
    unsigned int num;
    unsigned char ecount[AES_BLOCK_SIZE];
}ctr_state;

#define MAXLINE 4096
#define KEYLEN 17

void print_usage(char* prog_name);
void parse_args(int argc, char *argv[]);

int setup_addr_port(char *str_port, char* str_addr, sockaddr_in *socket_address);
int setup_client(sockaddr_in *socket_address);
void* read_client(void *arg);
void *write_client(void *arg);
int setup_server(sockaddr_in server_socket, sockaddr_in local_socket);
void* read_from_server(void *arg);
void init_ctr(ctr_state *state, const unsigned char iv[16]);
void* write_to_server(void *arg);

#endif