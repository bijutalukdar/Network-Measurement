
#include <pcap.h>

#include <string>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#include <getopt.h>
#include <string.h>

#include "ns_packet.h"


//-----------------------------------
//#define USE_PLAYBACK_TIMING
//-----------------------------------

int g_drop_pkt;

static pcap_t* pcap;
static char pcap_errbuf[PCAP_ERRBUF_SIZE];

typedef std::map<std::string, unsigned int> if_map_t;
static if_map_t if_map;

static ns_packet_device_h dev;
static unsigned int multiplier = 0;

static unsigned int card=0;
static unsigned int endpoint=1;

#ifdef USE_PLAYBACK_TIMING
static void _nsleep(time_t sec,long nsec=0)
{
  struct timespec T2,T1 = {sec,nsec};
  while (nanosleep(&T1,&T2)!=0 && errno==EINTR) {
    T1 = T2;
  }
}
#endif

static const void* process_next_packet(ssize_t* _length, unsigned int* _port)
{
  unsigned int port;
  ssize_t length;

  struct pcap_pkthdr h;
  const u_char* buf = pcap_next(pcap, &h);
  if (buf) {
    unsigned int offset = 0;
    int datalink = pcap_datalink(pcap);
    if (datalink == DLT_LINUX_SLL) { // linux "cooked" capture
      offset = 2;
      // fake an ethernet frame
      struct ethhdr* eth = (struct ethhdr*)(buf+offset);
      eth->h_proto = htons(ETH_P_IP);
      memcpy(eth->h_source, "SRCMAC", ETH_ALEN);
      memcpy(eth->h_dest, "DSTMAC", ETH_ALEN);
    }
    length = h.len-offset;

    // determine port number
    port = 0;

    std::string source((const char*)buf+6, 6);
    std::string dest((const char*)buf, 6);
    if_map_t::iterator itr;
    if ((itr = if_map.find(source)) == if_map.end()) {
      // see if dest is in if_map
      if ((itr = if_map.find(dest)) == if_map.end()) {
        //insert source and dest, choose source to be side "0"
        if_map[source] = 0;
        if_map[dest] = 1;
        port = 0;
      } else {
        port = itr->second ^ 1;
        if_map[source] = port;
      }
    } else {
      port = itr->second;
    }


#ifdef USE_PLAYBACK_TIMING
    // calculate time difference (max 1s)
    static struct timeval _prev = {0,0};
    unsigned int usec = 1000000L;
    if (_prev.tv_sec>0 && h.ts.tv_sec<=(_prev.tv_sec+1)) {
      // less than 2 seconds difference; calculate usec difference
      if (_prev.tv_sec == h.ts.tv_sec)
        usec = h.ts.tv_usec - _prev.tv_usec;
      else
        usec = (1000000L-_prev.tv_usec) + h.ts.tv_usec;

      if (usec > 1000000L)
        usec = 1000000L;
    }
    _prev = h.ts;

    // wait for previous packets to complete
    if (usec <= 1000000L) {
      usec *= multiplier;
      _nsleep(0, usec > 500L ? (usec*1000L) : 500L);
    }
    else {
      _nsleep(1);
    }
#endif

    *_length = length;
    *_port = port;
    return buf+offset;
  }
  return NULL;
}

ns_nfm_ret_t open_dev(unsigned int card, unsigned int endpoint, unsigned int host_id, ns_packet_device_h* devp)
{
  ns_packet_extra_options_t opt;

  memset(&opt, 0, sizeof(opt));
  opt.host_inline=1;
  return ns_packet_open_device_ex(devp, NFM_CARD_ENDPOINT_ID(card,endpoint,host_id),&opt);
}

static void usage(char* argv0)
{
  fprintf(stdout, "USAGE: %s <pcapfile>"
#ifdef USE_PLAYBACK_TIMING
                  " <options>\n\n"
                  "Options:\n"
                  "       -m,--multiplier[M]          Multiply PCAP time values by 'M' (default: 0)"
#endif
                  "\n",argv0);
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  if ((pcap = pcap_open_offline(argv[1], pcap_errbuf)) == NULL) {
    fprintf(stderr, "pcap_open_offline() failed: %s\n", pcap_errbuf);
    return 2;
  }

  if (open_dev(card,endpoint,NFM_ANY_ID,&dev)!=NS_NFM_SUCCESS) {
    fprintf(stderr, "Could not open device\n");
    return 3;
  }

  int long_argc = argc - 1;
  char** long_argv = argv + 1;

  static struct option long_options[] = {
    {"multiplier",    1, 0, 's'},
    {"help",          0, 0, 'h'},
    {0, 0, 0, 0}
  };

  int r;
  char* endptr;

  while ((r = getopt_long(long_argc, long_argv, "m:h", long_options, NULL)) != -1) {
    switch (r) {
      case 'm':
        multiplier = strtoul(optarg, &endptr, 0);
        if (*endptr != '\0') {
          fprintf(stderr, "Invalid multiplier '%s'\n", optarg);
          return 1;
        }
        break;
      case 'h':
      default:
        usage(argv[0]);
    }
  }

#ifdef USE_PLAYBACK_TIMING
  if (multiplier!=0)
    fprintf(stdout, "multiplier = %u\n", multiplier);
#endif

  const void* pkt;
  ssize_t length = 0;
  unsigned int port = 0;
  while ((pkt = process_next_packet(&length, &port))) {
    // inject packet
    ns_packet_t p;
    ns_packet_create(dev, length, &p);
    memcpy(p.packet_data, pkt, length);
    ns_packet_set_egress_port(&p, port);
    if (ns_packet_transmit(dev, &p, 0) != NS_NFM_SUCCESS) {
      fprintf(stderr, "Error sending packet");
      return 5;
    }
  }

  ns_packet_close_device(dev);
  pcap_close(pcap);

  printf("\n");
}
