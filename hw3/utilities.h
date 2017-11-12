#ifndef UTILITIES_H
#define UTILITIES_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>

typedef struct sockaddr_in sockaddr_in;
typedef struct hostent hostent;

#define MAXLINE 4096

void print_usage(char* prog_name);
void parse_args(int argc, char *argv[]);

int setup_addr_port(char *str_port, char* str_addr, sockaddr_in *socket_address);
int setup_client(sockaddr_in *socket_address);
void* read_client(void *arg);
void *write_client(void *arg);
int setup_server(sockaddr_in server_socket, sockaddr_in local_socket);
void* read_from_server(void *arg);
void* write_to_server(void *arg);

#endif