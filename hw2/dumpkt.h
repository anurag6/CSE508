#include <iostream>
#include <unistd.h>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <pcap.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h> 
#include <net/ethernet.h>
#include <netinet/ether.h> 
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip.h> 
#include <time.h>
#include <sstream>


#ifndef DUMPKT_H
#define DUMPKT_H

using namespace std;

void parse_args(int argc, char *argv[]);
void print_usage(string prog_name);
char* lookupDevice();
void process_packet(u_char *args,const struct pcap_pkthdr* pkthdr,const u_char* packet);
void printPacketDetails(const struct ether_header *eth_header, const struct ip *ip_header,struct timeval timestamp, u_int length);
void print_hex_ascii_line(const u_char *payload, int len, int offset);
void print_payload(const u_char *payload, int len);
#endif