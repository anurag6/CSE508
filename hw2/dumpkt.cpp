#include "dumpkt.h"


using namespace std;

extern int opterr;
extern int optopt;
extern int optind;
extern char *optarg;

char option;

char *device = NULL;
ostringstream expression;
char *search_str = NULL;

int iflag=0, rflag=0;

void print_usage(string prog_name)
{
	cerr << "\n"<<prog_name<<" [-i interface] [-r file] [-s string] expression \n"\
            "\n"                                                               \
            "Translates unicode files between utf-8, utf-16le, and utf-16be\n" \
            "\n"                                                               \
            "Option arguments:\n\n"                                            \
            "\t-h\t\tDisplays this usage menu.\n"                          \
            "\t-i\t\tLive capture from the network device <interface> (e.g., eth0).\n" \
            "\t-r\t\tReads packets from <file> in tcpdump format. \n"  \
            "\t-s\t\tKeeps only packets that contain <string> in their payload \n\t\t\t`(after any BPF filter is applied).\n";
}

void parse_args(int argc, char *argv[])
{
	while((option = getopt(argc, argv, ":hi:s:r:")) != -1)
	{
		switch(option)
		{
			case 'i':
			{
				if(iflag)
				{
					cerr<<"\nERROR: Duplicate i flag. Usage:\n\n";
					print_usage(argv[0]);
					exit(EXIT_FAILURE);
				}
				iflag = 1;
				device = optarg;	
				//cout<<"\nDEBUG_LOG:\t option \'i\' called for:"<<string(optarg);
				break;
			}
			case 'r':
			{
				if (rflag)
				{
					cerr<<"\nERROR: Duplicate r flag. Usage:\n\n";
					print_usage(argv[0]);
					exit(EXIT_FAILURE);
				}
				rflag = 1;
				//cout<<"\nDEBUG_LOG:\t option \'r\' called for:"<<string(optarg);
				device = optarg;
				break;
			}
			case 's':
			{
				cout<<"\nDEBUG_LOG:\t option \'s\' called.";
				search_str = optarg;
				break;
			}
			case 'h':
			{
				print_usage(string(argv[0]));
				exit(EXIT_SUCCESS);
				break;
			}
			case ':':
			{
				if(optopt == 'i')
				{
					//cerr<<"\nDEBUG_LOG:\t option \'i\' called for default argument.";
					iflag = 1;
					device = lookupDevice();
					//cerr<<"\nDEBUG_LOG:\t Sniffing on device:"<<string(device);
					break;
				}
				else
				{
					cerr<<"\n Option "<<optopt<" requires an operand.";
					exit(EXIT_FAILURE); 
				}
			}
			case '?':
			{
				cerr<<"\nERROR: Invalid or missing arguments. Usage:\n\n";
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
	if (!rflag && !iflag)
	{
		cerr<<"\nERROR: Invalid or missing arguments. Usage:\n\n";
		print_usage(argv[0]);
		exit(EXIT_FAILURE);		
	}
	cout<<"\nDEBUG_LOG:\t expression:";	
	for (int i = optind; i < argc; ++i)
	{
		if (argv[i] != NULL)
		{
			expression<<" "<<argv[i];
		}		
	}
	cout<<expression.str()<<"\n"; //TODO: DEBUG log
}

char* lookupDevice()
{
	char *device, errbuf[PCAP_ERRBUF_SIZE];
	device = pcap_lookupdev(errbuf);
	if (device == NULL)
	{
		cerr << "\nCouldn't find default device: "<<string(errbuf);
		exit(EXIT_FAILURE);
	}
	return device;
}

void process_packet(u_char *args,const struct pcap_pkthdr* pkthdr,const u_char*
        packet)
{
	//cerr << "\nDEBUG_LOG: process_packet called.";
    
    const struct ether_header *eth_header = NULL;
    const struct ip *ip_header = NULL;
    const u_char *payload = NULL;    
    const struct udphdr *udp_header = NULL;
    const struct tcphdr *tcp_header = NULL;

    u_int protocol_length=0;
    u_int type_len=0;
    u_int length = pkthdr->len;
    eth_header = (struct ether_header *) packet;
    if (ntohs (eth_header->ether_type) == ETHERTYPE_IP)
    {
	    ip_header = (struct ip*)(packet + sizeof(struct ether_header));
	    type_len = sizeof(struct ip);
	    switch(ip_header->ip_p)
	    {
	    	case IPPROTO_TCP:
	    		tcp_header = (struct tcphdr *)(packet + sizeof(struct ether_header) + type_len);
		    	protocol_length = tcp_header->th_off*4;
		    	break;
	    	case IPPROTO_UDP:
	    		udp_header = (struct udphdr *)(packet + sizeof(struct ether_header) + type_len);
	    		protocol_length = ntohs(udp_header->uh_ulen);
	    		break;
	    	case IPPROTO_ICMP:
	    		protocol_length = 8;
	    		break;
	    	default:
	    		break;
	    }
	}
    payload = (u_char *)(packet + sizeof(struct ether_header) + type_len + protocol_length);    
    int payload_length = length - (sizeof(struct ether_header) + type_len + protocol_length);
    if(search_str!= NULL)
    {
    	if(payload_length == 0)
    	{
    		return;
    	}
    	if(strstr((char *)payload,search_str) == NULL)
    	{
    		return;
    	}
    }
    printPacketDetails(eth_header, ip_header, pkthdr->ts, length);
    //cerr << "\nDEBUG_LOG: printPacketDetails len:"<<length<<" "<<sizeof(struct ether_header)<<" "<<type_len<<" "<<protocol_length;
    if (payload_length > 0)
    {
    	print_payload(payload, payload_length);
    }    
    cout<<endl;
}

void printPacketDetails(const struct ether_header *eth_header, const struct ip *ip_header,struct timeval timestamp, u_int length)
{
	//cerr << "\nDEBUG_LOG: printPacketDetails called.";
	char buf[100];
	ostringstream str;
	str.clear();
	str.str("");
	struct tm *ts;
	ts = localtime(&(timestamp.tv_sec));
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S:%d",ts);
	str << string(buf)<<":"<<timestamp.tv_usec;
	str<<" ";
	str<<string(ether_ntoa((const struct ether_addr *)&eth_header->ether_shost));
	str<<" -> ";
	str<<string(ether_ntoa((const struct ether_addr *)&eth_header->ether_dhost));
	str<<" type:";
	u_short ether_type = ntohs(eth_header->ether_type);;
	sprintf(buf,"0x%04x",ether_type);
	str<<string(buf);
	str<<" len:"<<length;
	cout<<"\n"<<str.str();
	str.clear();
	str.str("");
	if(ip_header!=NULL)
	{
		str<<"IP: "<<string(inet_ntoa(ip_header->ip_src))<<" -> "<<string(inet_ntoa(ip_header->ip_dst));
		str<<" Protocol: ";
		switch(ip_header->ip_p)
		{
			case IPPROTO_TCP:
				str<<"TCP";
				break;
			case IPPROTO_UDP:
				//cerr << "\nDEBUG_LOG: printPacketDetails got UDP. str:"<<str.str();
				str<<"UDP";
				break;
			case IPPROTO_ICMP:
				str<<"ICMP";
				break;
			default:
				str<<"0x"<<std::hex<<ip_header->ip_p;
		}		
		cout << "\n\t   "<<str.str();	
		//cout<<"\n"<<str.str();
		str.clear();
		str.str("");
	}
}

void
print_hex_ascii_line(const u_char *payload, int len, int offset)
{
	int i;
	int gap;
	const u_char *ch;

	/* offset */
	printf("\n\t   ");
	
	/* hex */
	ch = payload;
	for(i = 0; i < len; i++) {
		printf("%02x ", *ch);
		ch++;
		/* print extra space after 8th byte for visual aid */
		if (i == 7)
			printf(" ");
	}
	/* print space to handle line less than 8 bytes */
	if (len < 8)
		printf(" ");
	
	/* fill hex gap with spaces if not full line */
	if (len < 16) {
		gap = 16 - len;
		for (i = 0; i < gap; i++) {
			printf("   ");
		}
	}
	printf("   ");
	
	/* ascii (if printable) */
	ch = payload;
	for(i = 0; i < len; i++) {
		if (isprint(*ch))
			printf("%c", *ch);
		else
			printf(".");
		ch++;
	}
	return;
}

void
print_payload(const u_char *payload, int len)
{
	//cerr << "\nDEBUG_LOG: print_payload called. len:"<<len;
	int len_rem = len;
	int line_width = 16;			/* number of bytes per line */
	int line_len;
	int offset = 0;					/* zero-based offset counter */
	const u_char *ch = payload;

	if (len <= 0)
		return;

	/* data fits on one line */
	if (len <= line_width) {
		print_hex_ascii_line(ch, len, offset);
		return;
	}

	/* data spans multiple lines */
	for ( ;; ) {
		/* compute current line length */
		line_len = line_width % len_rem;
		/* print line */
		print_hex_ascii_line(ch, line_len, offset);
		/* compute total remaining */
		len_rem = len_rem - line_len;
		/* shift pointer to remaining bytes to print */
		ch = ch + line_len;
		/* add offset */
		offset = offset + line_width;
		/* check if we have line width chars or less */
		if (len_rem <= line_width) {
			/* print last line and get out */
			print_hex_ascii_line(ch, len_rem, offset);
			break;
		}
	}
	return;
}

int main(int argc, char *argv[])
{
	expression.clear();
	expression.str("");
	search_str = NULL;
	char errbuf[PCAP_ERRBUF_SIZE];
	parse_args(argc, argv);
	const char *filter_exp = (expression.str()).c_str();
	struct bpf_program comp_filter_exp;
	bpf_u_int32 mask;
	bpf_u_int32 net;
	pcap_t *handle = NULL;

	if(iflag)
	{
		if (pcap_lookupnet(device, &net, &mask, errbuf) == -1) 
		{
			cerr << "\nCan't get netmask for device "<< string(device);
			net = 0;
			mask = 0;
		}

		handle = pcap_open_live(device,BUFSIZ,0,-1,errbuf);
		if (handle == NULL)
		{
			cerr << "\nCouldn't open device " << string(device) <<": " << string(errbuf);
			exit(EXIT_FAILURE);
		}
	}
	else if(rflag)
	{
		handle = pcap_open_offline(device, errbuf);
		if (handle == NULL)
		{
			cerr << "\nCouldn't open file " << string(device) <<": " << string(errbuf);
			exit(EXIT_FAILURE);
		}
	}
	if (pcap_compile(handle, &comp_filter_exp, filter_exp, 0, net) == -1)
	{
		cerr << "\nCouldn't parse filter "<<string(filter_exp)<<": "<<string(pcap_geterr(handle));
		exit(EXIT_FAILURE);
	}

	if (pcap_setfilter(handle, &comp_filter_exp) == -1)
	{
		cerr << "\nCoulndn't install filter "<<string(filter_exp)<<": "<<string(pcap_geterr(handle));
		exit(EXIT_FAILURE);
	}
	
	/* And close the session */
	pcap_loop(handle,-1,process_packet,NULL);
	return 0;
}

