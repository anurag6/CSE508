all: pbproxy

pbproxy: pbproxy.c utilities.c
	gcc -o pbproxy pbproxy.c utilities.c -pthread -lm -lcrypto -lssl

clean:
	rm -f pbproxy
