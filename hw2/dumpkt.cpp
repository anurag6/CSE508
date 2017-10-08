#include <iostream>
#include <unistd.h>
#include <string>
# include <stdlib.h>
#include <pcap.h>

using namespace std;

extern int opterr;
extern int optopt;
extern int optind;
extern char *optarg;

char option;

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
	while((option = getopt(argc, argv, "hi::sr:")) != -1)
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
				if(optarg)	
				{
					cout<<"\nDEBUG_LOG:\t option \'i\' called for:"<<string(optarg);
				}
				else
				{
					cout<<"\nDEBUG_LOG:\t option \'i\' called with default argument.";
				}
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
				cout<<"\nDEBUG_LOG:\t option \'r\' called for:"<<string(optarg);
				break;
			}
			case 's':
			{
				cout<<"\nDEBUG_LOG:\t option \'s\' called.";
				break;
			}
			case 'h':
			{
				print_usage(string(argv[0]));
				exit(EXIT_SUCCESS);
				break;
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
			cout<<" "<<argv[i];
		}		
	}
	cout<<"\n";
}

int main(int argc, char *argv[])
{
	parse_args(argc, argv);
	char *device, errbuf[PCAP_ERRBUF_SIZE];
	device = pcap_lookupdev(errbuf);
	if (device == NULL)
	{
		cerr << "\nCouldn't find default device: "<<string(errbuf);
		exit(EXIT_FAILURE);
	}
	cout << "\nDEBUG_LOG: Device found:"<<string(device);
	return 0;
}

