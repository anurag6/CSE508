#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>
#include "utilities.h"


extern int opterr;
extern int optopt;
extern int optind;
extern char *optarg;



char option;

int kflag =0, lflag=0;

char *listen_port = NULL;

int start_server(struct sockaddr_in serv_addr, struct sockaddr_in ssh_addr);
void* server_call(int client_socket, sockaddr_in local_socket);

void print_usage(char* prog_name)
{
	fprintf(stderr, "\n%s [-l port] -k keyfile destination port\n"\
            "\n"                                                               \
            "Option arguments:\n"                                            \
            "    -l\tReverse-proxy mode: listen for inbound connections on <port> and relay\n"     \
            "      \tthem to <destination>:<port>\n" \
            "    -k\tUse the symmetric key contained in <keyfile> (as a hexadecimal string)\n", prog_name);
}

void parse_args(int argc, char *argv[])
{
	while((option = getopt(argc, argv, "hk:l:")) != -1)
	{
		switch(option)
		{
			case 'k':
			{
				if(kflag)
				{
					fprintf(stderr,"ERROR: Duplicate k flag. Usage:");
					print_usage(argv[0]);
					exit(EXIT_FAILURE);
				}
				kflag = 1;	
				fprintf(stderr,"DEBUG_LOG:\t option \'k\' called for:%s\n",(optarg));
				break;
			}
			case 'l':
			{
				if (lflag)
				{
					fprintf(stderr,"\nERROR: Duplicate l flag. Usage:\n\n");
					print_usage(argv[0]);
					exit(EXIT_FAILURE);
				}
				lflag = 1;
				fprintf(stderr,"DEBUG_LOG:\t option \'l\' called for:%s\n",(optarg));
				listen_port = optarg;
				break;
			}
			case 'h':
			{
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
				break;
			}
			case ':':
			{
				fprintf(stderr,"ERROR:Option %c requires an operand.\n",optopt);
				exit(EXIT_FAILURE); 
			
			}
			case '?':
			{
				fprintf(stderr,"ERROR: Invalid or missing arguments. Usage:\n\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
				break;
			}
			default:
			{
				exit(EXIT_FAILURE);
			}
		}
	}	
	fprintf(stderr,"DEBUG: non option args: ");
	char *addr=NULL, *port = NULL;
	for (int i = optind; i < argc; ++i)
	{
		if (argv[i] != NULL)
		{
			if(addr)
				port = argv[i];
			else
				addr = argv[i];
			fprintf(stderr,"%s %d, ",argv[i], i);
		}		
		
	}
	fprintf(stderr, "\n");
	if(lflag)
	{
		sockaddr_in server_address;
		if (setup_addr_port(port,addr, &server_address) < 0)
		{
			fprintf(stderr, "DEBUG: server_address setup failed.\n" );
			exit(EXIT_FAILURE);
		}

		//TODO:Debugging code
		char buf[20];
		inet_ntop(AF_INET,&(server_address.sin_addr),buf, 20);
		fprintf(stderr, "DEBUG: server address: %s port: %d\n", buf, ntohs(server_address.sin_port));
		
		sockaddr_in local_socket;
		if (setup_addr_port(listen_port, NULL, &local_socket) < 0)
		{
			fprintf(stderr, "DEBUG: local_socket setup failed.\n" );
			exit(EXIT_FAILURE);	
		}
		//TODO: Debug code
		inet_ntop(AF_INET,&(local_socket.sin_addr),buf, 20);
		fprintf(stderr, "DEBUG: local socket address: %s local socket port: %d\n", buf, ntohs(local_socket.sin_port));
		
		if (/*setup_server(server_address, local_socket)*/start_server(local_socket, server_address) < 0)
		{
			fprintf(stderr, "DEBUG: server setup failed.\n" );
			exit(EXIT_FAILURE);			
		}		
	}
	else
	{
		sockaddr_in socket_address;
		if (setup_addr_port(port,addr, &socket_address) < 0)
		{
			fprintf(stderr, "DEBUG: socket_address setup failed.\n" );
			exit(EXIT_FAILURE);
		}
		//TODO:Debugging code
		char buf[20];
		inet_ntop(AF_INET,&(socket_address.sin_addr),buf, 20);
		fprintf(stderr, "DEBUG: address: %s %d\n", buf, ntohs(socket_address.sin_port));
		if (setup_client(&socket_address) < 0)
		{
			fprintf(stderr, "DEBUG: client setup failed.\n" );
			exit(EXIT_FAILURE);		
		}
		
	}
	//cout<<expression.str()<<"\n"; //TODO: DEBUG log
}


int setup_addr_port(char *str_port, char* str_addr, sockaddr_in *socket_address)
{
	fprintf(stderr, "DEBUG: setup_addr_port port:%s addr:%s\n", str_port, str_addr);
	int port = atoi(str_port);

	hostent *d_server = NULL;
	if(str_addr != NULL)
	{	
		d_server = gethostbyname(str_addr);
		if(d_server == NULL)
		{
			fprintf(stderr, "ERROR: Couldn't open socket for destination server address:%s\n", str_addr);
			return -1;
		}
	}

	bzero((char*) socket_address, sizeof(*socket_address));
	socket_address->sin_family = AF_INET;
	if(str_addr == NULL)
		(socket_address->sin_addr).s_addr = htons(INADDR_ANY);
	else
		bcopy((char*)d_server->h_addr, (char*)(&(*socket_address).sin_addr.s_addr), d_server->h_length);
	socket_address->sin_port = htons(port);
	return 0;	
}

int setup_client(sockaddr_in *socket_address)
{
	fprintf(stderr, "DEBUG: setup_client called.\n");
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket < 0)
	{
		fprintf(stderr, "ERROR: Couldn't open socket.\n");
		return -1;
	}
	if (connect(client_socket, (struct sockaddr*)socket_address, sizeof(*socket_address)) < 0)
	{
		fprintf(stderr, "ERROR: Couldn't connect to server: %s.\n", strerror(errno) );
		return -1;
	}
	pthread_t read_th, write_th;
	void *fd_ptr = &client_socket;
	int read_status = pthread_create(&read_th,NULL,read_client, fd_ptr);
	if(read_status)
	{
		fprintf(stderr, "ERROR: Couldn't open read thread.\n" );
		return -1;
	}
	int write_status = pthread_create(&write_th,NULL,write_client,fd_ptr);
	if (write_status)
	{
		fprintf(stderr, "ERROR: Couldn't open write thread.\n" );
		return -1;
	}
	pthread_join(read_th, NULL);
	pthread_join(write_th, NULL);
	close(client_socket);
	return 0;
}

void* read_client(void *arg)
{
	fprintf(stderr, "DEBUG: read_client called.\n");
	int client_socket = *(int*)arg;
	char buffer[MAXLINE];

	ctr_state state;
	unsigned char _IV[16];
	AES_KEY aes_k;
	unsigned char* key = "1234567887654321";

	if (AES_set_encrypt_key(key, 128, &aes_k) < 0) 
	{
		fprintf(stderr,"AES_set_encrypt_key error.\n");
		pthread_exit(0);
	}

	while(1)
	{
		bzero(buffer,MAXLINE);
		/*int read_bytes = recv(client_socket,buffer,MAXLINE,0);
		if(read_bytes < 0)
		{
			//fprintf(stderr, "ERROR: Issue reading from socket. %s\n", strerror(errno));
			//close(client_socket);
			//exit(EXIT_FAILURE);			
		}
		else if(read_bytes > 0)
		{
			//fprintf(stderr, "DEBUG: read_client read %s.\n",buffer);
			write(STDOUT_FILENO, buffer, read_bytes);
		}*/
		int n;
		while ((n = read(client_socket, buffer, MAXLINE)) > 0) {
			/*if (n < 8) {
				fprintf(stderr, "Packet length smaller than 8!\n");
				close(sockfd);
				return 0;
			}*/

			memcpy(_IV, buffer, 8);
			unsigned char str[n - 8];
			init_ctr(&state, _IV);
			AES_ctr128_encrypt(buffer + 8, str, n - 8, &aes_k, state.ivec, state.ecount, &state.num);

			write(STDOUT_FILENO, str, n-8);

			if (n < MAXLINE)
				break;
		}
		//printf("Received message from client: %s\n", buffer);
	}
}

void* write_client(void *arg)
{
	fprintf(stderr, "DEBUG: write_client called.\n");
	int client_socket = *(int*)arg;
	char buffer[MAXLINE];

	ctr_state state;
	unsigned char _IV[16];
	AES_KEY aes_k;
	unsigned char* key = "1234567887654321";

	if (AES_set_encrypt_key(key, 128, &aes_k) < 0) 
	{
		fprintf(stderr,"AES_set_encrypt_key error.\n");
		pthread_exit(0);
	}
	/*fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

	int flags = fcntl(client_socket, F_GETFL);
	if (flags == -1) {
		printf("read sockfd flag error!\n");
		close(client_socket);
	}
	fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);*/
	while(1)
	{
		bzero(buffer,MAXLINE);
		/*if (read(STDIN_FILENO, buffer, MAXLINE) < 0)
		{
			//fprintf(stderr, "ERROR: Issue reading from STDIN.\n");
			//close(client_socket);
			//exit(EXIT_FAILURE);	
		}
		int write_bytes = send(client_socket, buffer, strlen(buffer), 0);
		if(write_bytes < 0)
		{
			//fprintf(stderr, "ERROR: Issue writing to socket.\n");
			//close(client_socket);
			//exit(EXIT_FAILURE);			
		}
		else if(write_bytes > 0)
		{
			//fprintf(stderr, "DEBUG: write_client wrote %s.\n",buffer);
		}*/
		int n;
		while ((n = read(STDIN_FILENO, buffer, MAXLINE)) > 0) 
		{
			
			if (!RAND_bytes(_IV, 8)) {
				fprintf(stderr,"Couldn't generate random bytes.\n");
				pthread_exit(0);
			}

			char *tmp = (char*)malloc(n + 8);
			memcpy(tmp, _IV, 8);
			unsigned char str[n];
			init_ctr(&state, _IV);
			AES_ctr128_encrypt(buffer, str, n, &aes_k, state.ivec, state.ecount, &state.num);
			memcpy(tmp + 8, str, n);
			
			write(client_socket, tmp, n+8);

			free(tmp);

			if (n < MAXLINE)
				break;
		}
	}
}


int start_server(struct sockaddr_in serv_addr, struct sockaddr_in ssh_addr)
{
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	pthread_t thread;
	pid_t pid;
	int status;


	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		fprintf(stderr, "ERROR: Couldn't open socket. %s\n", strerror(errno));

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		fprintf(stderr, "ERROR: Couldn't bind socket. %s\n", strerror(errno));
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	while (1) {
		//param = (thread_param *)malloc(sizeof(thread_param));
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		//param->sockfd = newsockfd;
		//param->ssh_addr = ssh_addr;
		//param->key = NULL;

		if (newsockfd > 0) {
			//pthread_create(&thread, 0, server_thread, (void *)param);
			//pthread_detach(thread);
			if ((pid=fork()) == 0)
			{
				//server_thread((void*)param);
				server_call(newsockfd,ssh_addr);
			}	
			else
			{
				wait(&status);
			}		
		} else {
			fprintf(stderr, "ERROR: Couldn't accept. %s\n", strerror(errno));
			//free(param);
		}
	}
	return 0; /* shouldn't get here */
}

void* read_from_server(void *arg)
{
	fprintf(stderr, "DEBUG: read_from_server called.\n");
	thread_arg argv = *(thread_arg*)arg;
	char buffer[MAXLINE];

}

void* write_to_server(void *arg)
{
	fprintf(stderr, "DEBUG: write_to_server called.\n");
	thread_arg argv = *(thread_arg*)arg;	
	char buffer[MAXLINE];
	
}

void* server_call(int client_socket, sockaddr_in local_socket)
{
	int n;
	int local_socket_fd, session_done = 0;
	unsigned char buffer[MAXLINE];

	bzero(buffer, MAXLINE);

	//if (!ptr) pthread_exit(0); 
	fprintf(stderr,"New thread started\n");
	//thread_param *params = (thread_param *)ptr;
	int sock = client_socket;
	struct sockaddr_in ssh_addr = local_socket;
	//unsigned char *key = params->key;
	

	local_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (connect(local_socket_fd, (struct sockaddr *)&local_socket, sizeof(local_socket)) < 0) 
	{
		fprintf(stderr,"Couldnt connect to server. %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	} 
	else 
	{
		fprintf(stderr,"Connected to server\n");
	}
	
	int flags = fcntl(client_socket, F_GETFL);
	if (flags == -1) {
		printf("read sock 1 flag error!\n");
		printf("Closing connections and exit thread!\n");
		close(client_socket);
		close(local_socket_fd);
		//free(params);
		exit(EXIT_FAILURE);
	}
	fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
	
	flags = fcntl(local_socket_fd, F_GETFL);
	if (flags == -1) {
		printf("read ssh_fd flag error!\n");
		close(client_socket);
		close(local_socket_fd);
		//free(params);
		exit(EXIT_FAILURE);
	}
	fcntl(local_socket_fd, F_SETFL, flags | O_NONBLOCK);

	flags = fcntl(STDIN_FILENO, F_GETFL);
	if (flags == -1) {
		printf("read ssh_fd flag error!\n");
		close(client_socket);
		close(local_socket_fd);
		//free(params);
		exit(EXIT_FAILURE);
	}
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	flags = fcntl(STDOUT_FILENO, F_GETFL);
	if (flags == -1) {
		printf("read ssh_fd flag error!\n");
		close(client_socket);
		close(local_socket_fd);
		//free(params);
		exit(EXIT_FAILURE);
	}
	fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);

	//ctr_state state;
	//AES_KEY aes_key;
	//unsigned char iv[8];
	
	//if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
	//	printf("Set encryption key error!\n");
	//	exit(1);
	//}

	ctr_state state;
	unsigned char _IV[16];
	unsigned char S_IV[16];
	AES_KEY aes_k;
	unsigned char* key = "1234567887654321";

	if (AES_set_encrypt_key(key, 128, &aes_k) < 0) 
	{
		fprintf(stderr,"AES_set_encrypt_key error.\n");
		pthread_exit(0);
	}

	while (1) 
	{
		//fprintf(stderr, "about to read from client\n");
		while ((n = read(client_socket, buffer, MAXLINE)) > 0) {
			//fprintf(stderr, "read from client:%s n:%d\n", buffer,n);
			//if (n < 8) {
			//	printf("Packet length smaller than 8!\n");
			//	close(sock);
			//	close(ssh_fd);
			//
			//	free(params);
			//	pthread_exit(0);
			//}
			
			memcpy(_IV, buffer, 8);
			unsigned char str[n-8];
			init_ctr(&state, _IV);
			AES_ctr128_encrypt(buffer+8, str, n-8, &aes_k, state.ivec, state.ecount, &state.num);
			
			//fprintf(stderr, "Read from client:%s\n",buffer);
			//fprintf(stderr, "writing _to_server:%s\n", buffer);
			write(/*local_socket_fd*/STDOUT_FILENO, str, n-8);
			//fprintf(stderr, "Done writing to server\n");
			//fprintf(stderr, "wrote_to_server:%s n:%d\n", buffer,n);

			if (n < MAXLINE)
				break;
		};
		
		//fprintf(stderr, "about to read from server\n");
		while ((n = read(/*local_socket_fd*/STDIN_FILENO, buffer, MAXLINE)) >= 0) {
			if (n > 0) {
				//fprintf(stderr, "read from _server:%s n:%d\n", buffer,n);
				if(!RAND_bytes(S_IV, 8)) 
				{
					fprintf(stderr, "Couldn't generate random bytes.\n");
					exit(1);
				}

				char *tmp = (char*)malloc(n + 8);
				memcpy(tmp, S_IV, 8);
				unsigned char str[n];
				init_ctr(&state, S_IV);
				AES_ctr128_encrypt(buffer, str, n, &aes_k, state.ivec, state.ecount, &state.num);
				memcpy(tmp+8, str, n);
				
				//usleep(900);
				//fprintf(stderr, "writing_to_client:%s\n", buffer);
				write(client_socket, tmp, n+8);
				//fprintf(stderr, "write_to_client:%s n:%d\n", buffer,n);
				free(tmp); 	
			}
			//printf("INFO: Sending data to ssh client\n");
			
			if (session_done == 0 && n == 0)
				session_done = 1;
			
			if (n < MAXLINE)
				break;
		}

		if (session_done == 1)
			break;
	}
	/*
	pthread_t read_th, write_th;
	void *fd_ptr = &client_socket;
	int read_status = pthread_create(&read_th,NULL,read_from_server, fd_ptr);
	if(read_status)
	{
		fprintf(stderr, "ERROR: Couldn't open read thread.\n" );
		return NULL;
	}
	int write_status = pthread_create(&write_th,NULL,write_to_server,fd_ptr);
	if (write_status)
	{
		fprintf(stderr, "ERROR: Couldn't open write thread.\n" );
		return NULL;
	}
	pthread_join(read_th, NULL);
	pthread_join(write_th, NULL);*/

	//printf("Closing connections. Exiting thread!\n");
	close(client_socket);
	close(local_socket_fd);
	//free(params);
	exit(0);
}

void init_ctr(ctr_state *state, const unsigned char iv[16])
{
	/* aes_ctr128_encrypt requires 'num' and 'ecount' set to zero on the
	* first call. */
	state->num = 0;
	memset(state->ecount, 0, AES_BLOCK_SIZE);

	/* Initialise counter in 'ivec' to 0 */
	memset(state->ivec + 8, 0, 8);

	/* Copy IV into 'ivec' */
	memcpy(state->ivec, iv, 8);
}