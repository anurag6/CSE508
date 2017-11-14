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

unsigned char* key = NULL;

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
				//fprintf(stderr,"DEBUG_LOG:\t option \'k\' called for:%s\n",(optarg));
				
				FILE *pFile;
				pFile = fopen ( optarg , "rb" );
			  	if (pFile==NULL) 
			  	{
			  		fprintf(stderr, "Error reading key file\n");
			  		exit (1);
			  	}

			  	// allocate memory to contain the whole file:
			  	key = (unsigned char*) malloc (sizeof(unsigned char)*(KEYLEN+1));
			  	if (key == NULL) 
			  	{
			  		fprintf(stderr, "Couldn't allocate mem for key\n");
			  		exit (1);
			  	}

			  	// copy the file into the buffer:
			  	int result = fread (key,1,KEYLEN,pFile);

			  	if (result != KEYLEN) 
			  	{	
			  		fprintf(stderr, "Key length not proper\n"); 
			  		exit(1);
			  	}
			  	//fprintf(stderr, "Key: %s\n",key);
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
	//			fprintf(stderr,"DEBUG_LOG:\t option \'l\' called for:%s\n",(optarg));
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
	//fprintf(stderr,"DEBUG: non option args: ");
	char *addr=NULL, *port = NULL;
	for (int i = optind; i < argc; ++i)
	{
		if (argv[i] != NULL)
		{
			if(addr)
				port = argv[i];
			else
				addr = argv[i];
	//		fprintf(stderr,"%s %d, ",argv[i], i);
		}		
		
	}
	if(!kflag)
	{
		fprintf(stderr,"ERROR: Missing key file input. Usage:\n\n");
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
	}
	//fprintf(stderr, "\n");
	if(lflag)
	{
		sockaddr_in server_address;
		if (setup_addr_port(port,addr, &server_address) < 0)
		{
			fprintf(stderr, "DEBUG: server_address setup failed.\n" );
			exit(EXIT_FAILURE);
		}

		//TODO:Debugging code
		//char buf[20];
		//inet_ntop(AF_INET,&(server_address.sin_addr),buf, 20);
		//fprintf(stderr, "DEBUG: server address: %s port: %d\n", buf, ntohs(server_address.sin_port));
		
		sockaddr_in local_socket;
		if (setup_addr_port(listen_port, NULL, &local_socket) < 0)
		{
			fprintf(stderr, "DEBUG: local_socket setup failed.\n" );
			exit(EXIT_FAILURE);	
		}
		//TODO: Debug code
		//inet_ntop(AF_INET,&(local_socket.sin_addr),buf, 20);
		//fprintf(stderr, "DEBUG: local socket address: %s local socket port: %d\n", buf, ntohs(local_socket.sin_port));
		
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
		//char buf[20];
		//inet_ntop(AF_INET,&(socket_address.sin_addr),buf, 20);
		//fprintf(stderr, "DEBUG: address: %s %d\n", buf, ntohs(socket_address.sin_port));
		if (setup_client(&socket_address) < 0)
		{
			fprintf(stderr, "ERROR: client setup failed.\n" );
			exit(EXIT_FAILURE);		
		}
		
	}
	//cout<<expression.str()<<"\n"; //TODO: DEBUG log
}


int setup_addr_port(char *str_port, char* str_addr, sockaddr_in *socket_address)
{
	//fprintf(stderr, "DEBUG: setup_addr_port port:%s addr:%s\n", str_port, str_addr);
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
	//fprintf(stderr, "DEBUG: setup_client called.\n");
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

	unsigned char C_IV[16];
	if (!RAND_bytes(C_IV, 8)) 
	{
		fprintf(stderr,"ERROR: Couldn't generate random bytes.\n");
		pthread_exit(0);
	}
	//fprintf(stderr, "IV being sent:%s\n", C_IV);
	int x = write(client_socket,C_IV,8);
	//fprintf(stderr, "IV sent:%s n:%d\n", C_IV,x);

	unsigned char S_IV[16];
	unsigned char buffer[MAXLINE];
	bzero(buffer,MAXLINE);
	x = read(client_socket,buffer,8);
	//fprintf(stderr, "Received S_IV:%s read:%d\n", buffer,x);
	memcpy(S_IV, buffer, 8);
	//fprintf(stderr, "Received S_IV%s\n", S_IV);	

	pthread_t read_th, write_th;
	
	client_thread cth;
	cth.fd = client_socket;	
	cth.CIV = S_IV;
	void *fd_ptr = &cth;
	int read_status = pthread_create(&read_th,NULL,read_client, fd_ptr);
	if(read_status)
	{
		fprintf(stderr, "ERROR: Couldn't open read thread.\n" );
		return -1;
	}

	client_thread cth2;
	cth2.fd = client_socket;
	cth2.CIV = C_IV;
	void *fd_ptr2 = & cth2;
	int write_status = pthread_create(&write_th,NULL,write_client,fd_ptr2);
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
	//fprintf(stderr, "DEBUG: read_client called.\n");
	client_thread cth = *(client_thread*)arg;
	int client_socket = cth.fd;
	unsigned char *S_IV = cth.CIV;
	unsigned char buffer[MAXLINE];

	ctr_state state;
	unsigned char _IV[16];
	AES_KEY aes_k;
	//unsigned char* key = "1234567887654321";

	if (AES_set_encrypt_key(key, 128, &aes_k) < 0) 
	{
		fprintf(stderr,"ERROR: AES_set_encrypt_key error.\n");
		pthread_exit(0);
	}

	while(1)
	{
		bzero(buffer,MAXLINE);
		int n;
		while ((n = read(client_socket, buffer, MAXLINE)) > 0) {
			//fprintf(stderr, "Received from server: %d\n", n);
			unsigned char str[n];
			init_ctr(&state, S_IV);
			//fprintf(stderr, "SIV:%s\n", S_IV);
			//fprintf(stderr, "Crypto received: %s\n", (buffer+8));
			//fprintf(stderr, "CIV :%s\n", C_IV);			
			AES_ctr128_encrypt(buffer, str, n, &aes_k, state.ivec, state.ecount, &state.num);
			//fprintf(stderr, "Wrote to stdout: %d\n", n);
			write(STDOUT_FILENO, buffer, n);

			if (n < MAXLINE)
				break;
		}
		//fprintf(stderr,"Received message from client: %s\n", buffer);
	}
}

void* write_client(void *arg)
{
	//fprintf(stderr, "DEBUG: write_client called.\n");
	client_thread cth = *(client_thread*)arg;
	int client_socket = cth.fd;
	unsigned char *S_IV = cth.CIV;
	unsigned char buffer[MAXLINE];

	ctr_state state;
	unsigned char C_IV[16];
	AES_KEY aes_k;
	//unsigned char* key = "1234567887654321";

	if (AES_set_encrypt_key(key, 128, &aes_k) < 0) 
	{
		fprintf(stderr,"ERROR: AES_set_encrypt_key error.\n");
		pthread_exit(0);
	}
	while(1)
	{
		bzero(buffer,MAXLINE);
		int n;
		while ((n = read(STDIN_FILENO, buffer, MAXLINE)) > 0) 
		{
			
			unsigned char str[n];
			init_ctr(&state, C_IV);
			//fprintf(stderr, "CIV:%s\n", S_IV);
			AES_ctr128_encrypt(buffer, str, n, &aes_k, state.ivec, state.ecount, &state.num);
			write(client_socket, buffer, n);

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
	{
		fprintf(stderr, "ERROR: Couldn't open socket. %s\n", strerror(errno));
		return -1;
	}

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		fprintf(stderr, "ERROR: Couldn't bind socket. %s\n", strerror(errno));
		return -1;
	}
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	while (1) {
		
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		fprintf(stderr,"Got a new connection\n");
		
		if (newsockfd > 0) {
		
			if ((pid=fork()) == 0)
			{
				server_call(newsockfd,ssh_addr);
			}	
			else
			{
				close(newsockfd);
			}		
		} 
		else 
		{
			fprintf(stderr, "ERROR: Couldn't accept. %s\n", strerror(errno));			
		}
	}
	return 0;
}

void* server_call(int client_socket, sockaddr_in local_socket)
{
	int n;
	int local_socket_fd, session_done = 0;
	unsigned char buffer[MAXLINE];
	int recv_CIV = 0;
	unsigned char C_IV[16];
	bzero(buffer, MAXLINE);

	
	//fprintf(stderr,"New process started.\n");	
	int sock = client_socket;
	struct sockaddr_in ssh_addr = local_socket;
	
	
	if (!recv_CIV)
	{
		n = read(client_socket,buffer,8);
		
		
		//fprintf(stderr, "Received C_IV:%s read:%d\n", buffer,n);
		memcpy(C_IV, buffer, 8);
		//fprintf(stderr, "Received C_IV%s\n", C_IV);
		recv_CIV = 1;
		
	}	
	unsigned char S_IV[16];
	if (!RAND_bytes(S_IV, 8)) 
	{
		fprintf(stderr,"ERROR: Couldn't generate random bytes.\n");
		pthread_exit(0);
	}
	//fprintf(stderr, "IV being sent:%s\n", C_IV);
	int x = write(client_socket,S_IV,8);
	//fprintf(stderr, "IV sent:%s n:%d\n", S_IV,x);

	local_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (connect(local_socket_fd, (struct sockaddr *)&local_socket, sizeof(local_socket)) < 0) 
	{
		fprintf(stderr,"ERROR: Couldnt connect to server. %s\n",strerror(errno));
		exit(EXIT_FAILURE);
	} 
	else 
	{
		//fprintf(stderr,"DEBUG: Connected to server\n");
	}
	
	int flags = fcntl(client_socket, F_GETFL);
	if (flags == -1) 
	{
		fprintf(stderr,"ERROR: client_socket flag error.\n");		
		close(client_socket);
		close(local_socket_fd);
		//free(params);
		exit(EXIT_FAILURE);
	}
	fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
	
	flags = fcntl(local_socket_fd, F_GETFL);
	if (flags == -1) 
	{
		fprintf(stderr,"ERROR: local_socket flag error.\n");
		close(client_socket);
		close(local_socket_fd);
		//free(params);
		exit(EXIT_FAILURE);
	}
	fcntl(local_socket_fd, F_SETFL, flags | O_NONBLOCK);

	ctr_state state;
	AES_KEY aes_k;
	//unsigned char* key = "1234567887654321";

	if (AES_set_encrypt_key(key, 128, &aes_k) < 0) 
	{
		fprintf(stderr,"AES_set_encrypt_key error.\n");
		pthread_exit(0);
	}
	
	//fprintf(stderr, "Moving on to normal code.\n");	
	while (1) 
	{
		bzero(buffer, MAXLINE);
		//fprintf(stderr, "about to read from client\n");
		while ((n = read(client_socket, buffer, MAXLINE)) > 0) 
		{			
			
			unsigned char str[n];
			bzero(str,n);
			init_ctr(&state, C_IV);			
			AES_ctr128_encrypt(buffer, str, n, &aes_k, state.ivec, state.ecount, &state.num);
						
			int x = write(local_socket_fd/*STDOUT_FILENO*/, buffer, n);
			//if(x != n)
			//{
			//	fprintf(stderr, "x!=n");
			//}
			//fprintf(stderr, "Done writing to server\n");
			//fprintf(stderr, "wrote_to_server: n:%d x%d\n\n", n,x);
			//fprintf(stderr, "%s\n", str);

			if (n < MAXLINE)
				break;
			
		};
		if(n==0)
			break;
		bzero(buffer, MAXLINE);
		//fprintf(stderr, "about to read from server\n");
		while ((n = read(local_socket_fd/*STDIN_FILENO*/, buffer, MAXLINE)) >= 0) {
			if (n > 0) {
				//fprintf(stderr, "read from _server:%s n:%d\n", buffer,n);
				//fprintf(stderr, "Received from server: %d\n", n);
				unsigned char str[n];
				bzero(str,n);
				init_ctr(&state, S_IV);
				//fprintf(stderr, "SIV:%s\n", S_IV);
				AES_ctr128_encrypt(buffer, str, n, &aes_k, state.ivec, state.ecount, &state.num);
				
				//fprintf(stderr, "No usleep\n");
				//fprintf(stderr, "writing_to_client:%s\n", buffer);
				
				int x = write(client_socket, buffer, n);
				usleep(900);
				//fprintf(stderr, "write_to_client:n:%d x:%d\n\n", n,x);
				
			}
			
			
			if (session_done == 0 && n == 0)
				session_done = 1;
			
			if (n < MAXLINE)
				break;
		}

		if (session_done == 1)
			break;
	}
	
	fprintf(stderr,"Closing connection. \n");
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