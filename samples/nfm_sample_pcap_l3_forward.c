
#include <pcap.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>


#include "ns_packet.h"

#define NFM_ENDPOINT    1


void nfm_eck(const char *s, ns_nfm_ret_t ret)
{
    if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "%s\n", s);
        exit(-1);
    }
}


void usage(const char *progname)
{
    printf("usage: %s [-n nfp] [-v vrid] pcapfile\n", progname);
    exit(1);
}


int main(int argc, char *argv[])
{
    pcap_t *pcap;
    ns_packet_device_h dev;
    int r;
    const char *pfn;
    int nfe = 0;
    char errbuf[PCAP_ERRBUF_SIZE];
    ns_packet_extra_options_t opt;
    ns_nfm_ret_t ret;
    unsigned int nfmid;
    struct pcap_pkthdr ph;
    const u_char *buf;
    ns_packet_t p;
    unsigned int vrid = 1;

    while ((r = getopt(argc, argv, "n:v:")) >= 0) {
        switch (r) {
        case 'c':
            nfe = atoi(optarg);
            break;

        case 'v':
            vrid = atoi(optarg);
            break;

        default:
            usage(argv[0]);
        }

        if (optind == argc)
            usage(argv[0]);
        pfn = argv[optind];
    }


    if ((pcap = pcap_open_offline(pfn, errbuf)) == NULL) {
        fprintf(stderr, "Error openining pcap file '%s': %s\n", pfn, errbuf);
        return -1;
    }
    memset(&opt, 0, sizeof(opt));
    opt.host_inline = 1;
    nfmid = NFM_CARD_ENDPOINT_ID(nfe, NFM_ENDPOINT, NFM_ANY_ID);
    ret = ns_packet_open_device(&dev, nfmid);
    nfm_eck("Error opening device", ret);


    while ((buf = pcap_next(pcap, &ph)) != NULL) {
        ret = ns_packet_create(dev, ph.caplen, &p);
        nfm_eck("Error creating packet\n", ret);
        memcpy(p.packet_data, buf, ph.caplen);
        ret = ns_packet_l3_forward(dev, &p, 0, vrid);
        nfm_eck("Error l3 forwarding packet\n", ret);
    }

    ns_packet_close_device(dev);
    pcap_close(pcap);

    return 0;
}
