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
#include "utilities.h"


extern int opterr;
extern int optopt;
extern int optind;
extern char *optarg;

typedef struct
{
	int client_fd;
	int target_fd;
} thread_arg;

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

	/*fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

	int flags = fcntl(client_socket, F_GETFL);
	if (flags == -1) {
		printf("read sockfd flag error!\n");
		close(client_socket);
	}
	fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);*/
	while(1)
	{
		/*bzero(buffer,MAXLINE);
		int read_bytes = recv(client_socket,buffer,MAXLINE,0);
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

			//memcpy(iv, buffer, 8);
			//unsigned char decryption[n - 8];
			//init_ctr(&state, iv);
			//AES_ctr128_encrypt(buffer + 8, decryption, n - 8, &aes_key, state.ivec, state.ecount, &state.num);

			write(STDOUT_FILENO, buffer, n);

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
			/*
			if (!RAND_bytes(iv, 8)) {
				printf("Error generating random bytes.\n");
				exit(1);
			}

			char *tmp = (char*)malloc(n + 8);
			memcpy(tmp, iv, 8);
			unsigned char encryption[n];
			init_ctr(&state, iv);
			AES_ctr128_encrypt(buffer, encryption, n, &aes_key, state.ivec, state.ecount, &state.num);
			memcpy(tmp + 8, encryption, n);
			*/
			write(client_socket, buffer, n);

			//free(tmp);

			if (n < MAXLINE)
				break;
		}
	}
}

typedef struct {
	int sockfd;
	struct sockaddr_in ssh_addr;
	unsigned char *key;
} thread_param;

void* server_thread(void *ptr);

int setup_server(sockaddr_in server_socket, sockaddr_in local_socket)
{
	int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket_fd < 0)
	{
		fprintf(stderr, "ERROR: Couldn't open socket. %s\n", strerror(errno));
		return -1;
	}
	int client_socket;
	int local_socket_fd;
	pid_t pid;
	pthread_t read_th, write_th, thread;
	int fork_status;

	//TODO: Debug code
	char buf[20];
	inet_ntop(AF_INET,&(local_socket.sin_addr),buf, 20);
	fprintf(stderr, "DEBUG: local socket address: %s local socket port: %d\n", buf, ntohs(local_socket.sin_port));
	
	//TODO: Debug code
	inet_ntop(AF_INET,&(server_socket.sin_addr),buf, 20);
	fprintf(stderr, "DEBUG: server socket address: %s server socket port: %d\n", buf, ntohs(server_socket.sin_port));
		


	if(bind(server_socket_fd, (struct sockaddr*) &local_socket, sizeof(local_socket)) < 0)
	{
		fprintf(stderr, "ERROR: Couldn't bind socket. %s\n", strerror(errno));
		return -1;	
	}
	sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	listen(server_socket_fd, 8);
	while(1)
	{
		thread_param *param = NULL;
		param = (thread_param *)malloc(sizeof(thread_param));
		client_socket = accept(server_socket_fd, (struct sockaddr*)&client_addr,&client_addr_len);
		param->sockfd = client_socket;
		param->ssh_addr = local_socket;
		param->key = NULL;
		if(client_socket > 0)
		{
			//process client
			if((pid = fork()) == 0)
			{/* 	
				unsigned char buffer[MAXLINE];
				bzero(buffer, MAXLINE);
				local_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
				if(connect(local_socket_fd, (struct sockaddr*)&local_socket, sizeof(local_socket)) < 0)
				{
					fprintf(stderr, "ERROR: Couldn't connect to server. %s\n", strerror(errno));
					exit(EXIT_FAILURE);				
				}
				else
					fprintf(stderr, "DEBUG: Connected to server\n");*/
				/*thread_arg th;
				th.client_fd = client_socket;
				th.target_fd = local_socket_fd;
				void *arg = &th;
				int read_status = pthread_create(&read_th,NULL,read_from_server, arg);
				if(read_status)
				{
					fprintf(stderr, "ERROR: Couldn't open read thread.\n" );
					return -1;
				}
				int write_status = pthread_create(&write_th,NULL,write_to_server,arg);
				if (write_status)
				{
					fprintf(stderr, "ERROR: Couldn't open write thread.\n" );
					return -1;
				}
				pthread_join(read_th, NULL);
				pthread_kill(write_th, SIGINT);
				pthread_join(write_th, NULL);
				close(client_socket);
				close(local_socket_fd);*/
				//server_thread((void*)param);
				server_call(client_socket,local_socket);
			}
			else
			{
				wait(&fork_status);
				//close(client_socket);
			}/*
			fprintf(stderr, "accepted connection\n");
			thread_param * param = (thread_param*)malloc(sizeof(thread_param));
			param->sockfd = client_socket;
			param->ssh_addr = server_socket;
			param->key = NULL;
			pthread_create(&thread, 0, server_thread, (void*)param);
			pthread_detach(thread);*/
			return 0;
		}
		else
		{
			fprintf(stderr, "ERROR: Couldn't accept connection. %s\n",strerror(errno));
			free(param);
			return -1;			
		}
	}
}

int start_server(struct sockaddr_in serv_addr, struct sockaddr_in ssh_addr)
{
	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in cli_addr;
	thread_param *param;
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
		param = (thread_param *)malloc(sizeof(thread_param));
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		param->sockfd = newsockfd;
		param->ssh_addr = ssh_addr;
		param->key = NULL;

		if (newsockfd > 0) {
			//pthread_create(&thread, 0, server_thread, (void *)param);
			//pthread_detach(thread);
			if ((pid=fork()) == 0)
			{
				server_thread((void*)param);
			}	
			else
			{
				wait(&status);
			}		
		} else {
			fprintf(stderr, "ERROR: Couldn't accept. %s\n", strerror(errno));
			free(param);
		}
	}
	return 0; /* shouldn't get here */
}

void* read_from_server(void *arg)
{
	unsigned char buffer[MAXLINE];
	int session_closed = 0;
	bzero(buffer, MAXLINE);
	thread_arg argv = *(thread_arg*)arg;
	while(1)
	{
		int n;
		while((n=read(argv.target_fd, buffer, MAXLINE)) >= 0)
		{
			fprintf(stderr, "read_from_server:%s n:%d\n", buffer,n);
			if(n > 0)
			{
				write(argv.client_fd, buffer, n);

			}
			if(n==0 && session_closed == 0)
				session_closed = 1;
			if(n<MAXLINE)
			{
				fprintf(stderr, "DEBUG: Exiting read while\n");
				break;
			}
		}
		if (session_closed == 1)
		{
			fprintf(stderr, "DEBUG: read_from_server Exiting forever while\n");
			break;
		}
	}
}

void* write_to_server(void *arg)
{
	unsigned char buffer[MAXLINE];
	bzero(buffer, MAXLINE);
	thread_arg argv = *(thread_arg*)arg;
	while(1)
	{
		int n;
		while((n=read(argv.client_fd, buffer, MAXLINE)) > 0)
		{
			fprintf(stderr, "write_to_server:%s n:%d\n", buffer,n);
			write(argv.target_fd,buffer,n);
			if(n < MAXLINE)
				break;
			fprintf(stderr, "write_to_server next iteration.\n");
		}
	}
}

void* server_thread(void *ptr)
{
	int n;
	int ssh_fd, ssh_done = 0;
	unsigned char buffer[MAXLINE];

	bzero(buffer, MAXLINE);

	if (!ptr) pthread_exit(0); 
	printf("New thread started\n");
	thread_param *params = (thread_param *)ptr;
	int sock = params->sockfd;
	struct sockaddr_in ssh_addr = params->ssh_addr;
	unsigned char *key = params->key;
	

	ssh_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (connect(ssh_fd, (struct sockaddr *)&ssh_addr, sizeof(ssh_addr)) < 0) {
		printf("Connection to ssh failed!\n");
		pthread_exit(0);
	} else {
		printf("Connection to ssh established!\n");
	}
	
	int flags = fcntl(sock, F_GETFL);
	if (flags == -1) {
		printf("read sock 1 flag error!\n");
		printf("Closing connections and exit thread!\n");
		close(sock);
		close(ssh_fd);
		free(params);
		pthread_exit(0);
	}
	fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	
	flags = fcntl(ssh_fd, F_GETFL);
	if (flags == -1) {
		printf("read ssh_fd flag error!\n");
		close(sock);
		close(ssh_fd);
		free(params);
		pthread_exit(0);
	}
	fcntl(ssh_fd, F_SETFL, flags | O_NONBLOCK);

	//ctr_state state;
	//AES_KEY aes_key;
	//unsigned char iv[8];
	
	/*if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
		printf("Set encryption key error!\n");
		exit(1);
	}*/

	while (1) {
		while ((n = read(sock, buffer, MAXLINE)) > 0) {
			fprintf(stderr, "read from client:%s n:%d\n", buffer,n);
			/*if (n < 8) {
				printf("Packet length smaller than 8!\n");
				close(sock);
				close(ssh_fd);

				free(params);
				pthread_exit(0);
			}*/
			
			//memcpy(iv, buffer, 8);
			//unsigned char decryption[n-8];
			//init_ctr(&state, iv);
			//AES_ctr128_encrypt(buffer+8, decryption, n-8, &aes_key, state.ivec, state.ecount, &state.num);
			//printf("%.*s\n", n, buffer);
			fprintf(stderr, "writing _to_server:%s\n", buffer);
			write(ssh_fd, buffer, n);
			fprintf(stderr, "wrote_to_server:%s n:%d\n", buffer,n);

			if (n < MAXLINE)
				break;
		};
		
		while ((n = read(ssh_fd, buffer, MAXLINE)) >= 0) {
			if (n > 0) {
				fprintf(stderr, "read from _server:%s n:%d\n", buffer,n);
				/*if(!RAND_bytes(iv, 8)) {
					fprintf(stderr, "Error generating random bytes.\n");
					exit(1);
				}*/

				//char *tmp = (char*)malloc(n + 8);
				//memcpy(tmp, iv, 8);
				//unsigned char encryption[n];
				//init_ctr(&state, iv);
				//AES_ctr128_encrypt(buffer, encryption, n, &aes_key, state.ivec, state.ecount, &state.num);
				//memcpy(tmp+8, encryption, n);
				
				//usleep(900);
				fprintf(stderr, "writing_to_client:%s\n", buffer);
				write(sock, buffer, n);
				fprintf(stderr, "write_to_client:%s n:%d\n", buffer,n);
				//free(tmp);
			}
			printf("INFO: Sending data to ssh client\n");
			
			if (ssh_done == 0 && n == 0)
				ssh_done = 1;
			
			if (n < MAXLINE)
				break;
		}

		if (ssh_done == 1)
			break;
	}

	printf("Closing connections. Exiting thread!\n");
	close(sock);
	close(ssh_fd);
	free(params);
	exit(0);
}

void* server_call(int client_socket, sockaddr_in local_socket)
{
	int n;
	int local_socket_fd, session_done = 0;
	unsigned char buffer[MAXLINE];

	bzero(buffer, MAXLINE);

	//if (!ptr) pthread_exit(0); 
	printf("New thread started\n");
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

	//ctr_state state;
	//AES_KEY aes_key;
	//unsigned char iv[8];
	
	/*if (AES_set_encrypt_key(key, 128, &aes_key) < 0) {
		printf("Set encryption key error!\n");
		exit(1);
	}*/

	while (1) {
		while ((n = read(client_socket, buffer, MAXLINE)) > 0) {
			//fprintf(stderr, "read from client:%s n:%d\n", buffer,n);
			/*if (n < 8) {
				printf("Packet length smaller than 8!\n");
				close(sock);
				close(ssh_fd);

				free(params);
				pthread_exit(0);
			}*/
			
			//memcpy(iv, buffer, 8);
			//unsigned char decryption[n-8];
			//init_ctr(&state, iv);
			//AES_ctr128_encrypt(buffer+8, decryption, n-8, &aes_key, state.ivec, state.ecount, &state.num);
			//printf("%.*s\n", n, buffer);
			//fprintf(stderr, "writing _to_server:%s\n", buffer);
			write(local_socket_fd, buffer, n);
			//fprintf(stderr, "wrote_to_server:%s n:%d\n", buffer,n);

			if (n < MAXLINE)
				break;
		};
		
		while ((n = read(local_socket_fd, buffer, MAXLINE)) >= 0) {
			if (n > 0) {
				//fprintf(stderr, "read from _server:%s n:%d\n", buffer,n);
				/*if(!RAND_bytes(iv, 8)) {
					fprintf(stderr, "Error generating random bytes.\n");
					exit(1);
				}*/

				//char *tmp = (char*)malloc(n + 8);
				//memcpy(tmp, iv, 8);
				//unsigned char encryption[n];
				//init_ctr(&state, iv);
				//AES_ctr128_encrypt(buffer, encryption, n, &aes_key, state.ivec, state.ecount, &state.num);
				//memcpy(tmp+8, encryption, n);
				
				//usleep(900);
				//fprintf(stderr, "writing_to_client:%s\n", buffer);
				write(client_socket, buffer, n);
				//fprintf(stderr, "write_to_client:%s n:%d\n", buffer,n);
				//free(tmp);
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

	//printf("Closing connections. Exiting thread!\n");
	close(client_socket);
	close(local_socket_fd);
	//free(params);
	exit(0);
}

