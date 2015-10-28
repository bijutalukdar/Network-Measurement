/**
 * Copyright (C) 2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_flowstats.c
 * Description: Sample application to demonstrate start/end of flow using the packet API.
 */

#include "ns_packet.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/time.h>
#include <errno.h>

#include "ns_flow.h"


#define print_error(r, prefix)  fprintf(stderr, "%s: %s: %s. (subcode=%d).\n", prefix, ns_nfm_module_string(r), ns_nfm_error_string(r), NS_NFM_ERROR_SUBCODE(r))

volatile int running = 1;
static int change_context=0;
static int context=0;
static int enable_eof=0;
static int change_eof=0;
ns_packet_device_h dev;

void sig_term(int __attribute__((unused)) dummy)
{
  running = 0;
}

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  " -e --endpoint n Select endpoint (default 1)\n"
                  " -i --host_id i  Set host destination id (default 15)\n"
                  " -m --multi d.e.i[:d.e.i]... Receive from multiple device.endpoint.id tuples (ignore -d, -e and -i options)\n"
                  " -c --context n  Change the flow opaque context (in start of flow callback) to n\n"
                  " -x --eof n      Modify the flow (in start of flow callback) to enable/disable end of flow message from NPU\n"
          ,argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"host_id",   1, 0, 'i'},
  {"endpoint",  1, 0, 'e'},
  {"device",    1, 0, 'd'},
  {"multi",     1, 0, 'm'},
  {"context",   1, 0, 'c'},
  {"eof",       1, 0, 'x'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

#define ipv4_printf_format "%u.%u.%u.%u"
#define ipv4_printf_le(x) ((x)>>24), ((x)>>16)&0xff, ((x)>>8)&0xff, (x)&0xff
#define ipv4_printf_be(x) (x)&0xff, ((x)>>8)&0xff, ((x)>>16)&0xff, ((x)>>24)
#define ipv6_printf_format "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define ipv6_printf(x) x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15]

void sof_callback(const ns_packet_flow_start_stats_t* sof_stats, uint64_t cb_context __attribute__((unused))) {
  printf("Start of flow: flow ID=%u, context=%u, ingress %s interface=%u, egress %s interface=%u\n",
      sof_stats->flow_id, sof_stats->ctx,
      (sof_stats->ingress_if_type==0)?"physical":"logical", sof_stats->ingress_interface,
      (sof_stats->egress_if_type==0)?"physical":"logical", sof_stats->egress_interface);
  if (sof_stats->flow_type==0) {
    // IPv4
    printf("src IP "ipv4_printf_format", dst IP "ipv4_printf_format", protocol %hhu, %cVLAN %u, addr space id %u",
        ipv4_printf_be(sof_stats->IPv4.src), ipv4_printf_be(sof_stats->IPv4.dst),
        sof_stats->IPv4.protocol, (sof_stats->IPv4.vlan_c_not_s)?'c':'s', sof_stats->IPv4.vlan_id, sof_stats->IPv4.addr_space_id);
    if (sof_stats->IPv4.protocol==IPPROTO_UDP || sof_stats->IPv4.protocol==IPPROTO_TCP) {
      printf(", src port %hu, dst port %hu\n", ntohs(sof_stats->IPv4.L4_srcport), ntohs(sof_stats->IPv4.L4_dstport));
    } else {
      printf("\n");
    }
  } else {
    // IPv6
    printf("src IP "ipv6_printf_format", dst IP "ipv6_printf_format", protocol %hhu, %cVLAN %u, addr space id %u",
        ipv6_printf(sof_stats->IPv6.src), ipv6_printf(sof_stats->IPv6.dst),
        sof_stats->IPv6.protocol, (sof_stats->IPv6.vlan_c_not_s)?'c':'s', sof_stats->IPv6.vlan_id, sof_stats->IPv6.addr_space_id);
    if (sof_stats->IPv6.protocol==IPPROTO_UDP || sof_stats->IPv6.protocol==IPPROTO_TCP) {
      printf(", src port %hu, dst port %hu\n", ntohs(sof_stats->IPv6.L4_srcport), ntohs(sof_stats->IPv6.L4_dstport));
    } else {
      printf("\n");
    }
  };
  printf("flow start time: %u.%06us\n", sof_stats->flow_sof_timestamp_s, sof_stats->flow_sof_timestamp_us);

  if (change_eof || change_context) {
    ns_flow_modifier_h fm;
    if (sof_stats->flow_type==0) {
      if (ns_flow_create_ntuple_modifier(dev, sof_stats->IPv4.src, sof_stats->IPv4.dst, sof_stats->IPv4.L4_srcport, sof_stats->IPv4.L4_dstport, sof_stats->IPv4.protocol, sof_stats->IPv4.addr_space_id, &fm)==NS_NFM_SUCCESS) {
        if (change_eof) ns_flow_set_eof_statistics(fm, (enable_eof>0)?1:0);
        if (change_context) ns_flow_set_user_context(fm, context);
        ns_flow_apply_modifier(fm);
      }
    } else {
      if (ns_flow_create_ipv6_ntuple_modifier(dev, (const char*)sof_stats->IPv6.src, (const char*)sof_stats->IPv6.dst, sof_stats->IPv6.L4_srcport, sof_stats->IPv6.L4_dstport, sof_stats->IPv6.protocol, sof_stats->IPv6.addr_space_id, &fm)==NS_NFM_SUCCESS) {
        if (change_eof) ns_flow_set_eof_statistics(fm, (enable_eof>0)?1:0);
        if (change_context) ns_flow_set_user_context(fm, context);
        ns_flow_apply_modifier(fm);
      }
    }
  }
}

void eof_callback(const ns_packet_flow_end_stats_t* eof_stats, uint64_t cb_context __attribute__((unused))) {
  printf("End of flow: flow ID=%u, context=%u, rule_id=%hu, ingress %s interface=%u, egress %s interface=%u, total packets=%llu, total bytes=%llu\n",
      eof_stats->flow_id, eof_stats->ctx, eof_stats->rule_id,
      (eof_stats->ingress_if_type==0)?"physical":"logical", eof_stats->ingress_interface,
      (eof_stats->egress_if_type==0)?"physical":"logical", eof_stats->egress_interface,
      (unsigned long long)(eof_stats->packet_count[0]+eof_stats->packet_count[1]),
      (unsigned long long)(eof_stats->byte_count[0]+eof_stats->byte_count[1]));
  if (eof_stats->flow_type==0) {
    // IPv4
    printf("src IP "ipv4_printf_format", dst IP "ipv4_printf_format", protocol %hhu, %cVLAN %u, addr space id %u",
        ipv4_printf_be(eof_stats->IPv4.src), ipv4_printf_be(eof_stats->IPv4.dst),
        eof_stats->IPv4.protocol, (eof_stats->IPv4.vlan_c_not_s)?'c':'s', eof_stats->IPv4.vlan_id, eof_stats->IPv4.addr_space_id);
    if (eof_stats->IPv4.protocol==IPPROTO_UDP || eof_stats->IPv4.protocol==IPPROTO_TCP) {
      printf(", src port %hu, dst port %hu\n", ntohs(eof_stats->IPv4.L4_srcport), ntohs(eof_stats->IPv4.L4_dstport));
    } else {
      printf("\n");
    }
  } else {
    // IPv6
    printf("src IP "ipv6_printf_format", dst IP "ipv6_printf_format", protocol %hhu, %cVLAN %u, addr space id %u",
        ipv6_printf(eof_stats->IPv6.src), ipv6_printf(eof_stats->IPv6.dst),
        eof_stats->IPv6.protocol, (eof_stats->IPv6.vlan_c_not_s)?'c':'s', eof_stats->IPv6.vlan_id, eof_stats->IPv6.addr_space_id);
    if (eof_stats->IPv6.protocol==IPPROTO_UDP || eof_stats->IPv6.protocol==IPPROTO_TCP) {
      printf(", src port %hu, dst port %hu\n", ntohs(eof_stats->IPv6.L4_srcport), ntohs(eof_stats->IPv6.L4_dstport));
    } else {
      printf("\n");
    }
  };
  printf("flow start time: %u.%06us\n", eof_stats->flow_sof_timestamp_s, eof_stats->flow_sof_timestamp_us);
  printf("flow end time: %u.%06us\n", eof_stats->flow_eof_timestamp_s, eof_stats->flow_eof_timestamp_us);
}

int main(int argc, char **argv)
{
  ns_nfm_ret_t r;
  ns_packet_t pckt;
  unsigned int host_id = 15;
  unsigned int endpoint = 1;
  unsigned int device = 0;
  unsigned int flags = 0;
  unsigned int* multi=0;
  unsigned int num_multi=0;
  unsigned int index;
  char* opt=0;
  char* tok=0;

  int c;
  while ((c = getopt_long(argc, argv, "hi:d:e:m:c:x:", __long_options, NULL)) != -1) {
    switch (c) {
    case 'i':
      host_id = (unsigned int)strtoul(optarg,0,0);
      if (host_id > 31) {
        fprintf(stderr, "Host_id %d is out of range (0-31)\n", host_id);
        exit(1);
      }
      break;
    case 'd':
      device = (unsigned int)strtoul(optarg,0,0);
      if (device > 3) {
        fprintf(stderr, "Device %d is out of range (0-3)\n", device);
        exit(1);
      }
      break;
    case 'e':
      endpoint = (unsigned int)strtoul(optarg,0,0);
      if (endpoint > NFM_MAX_ENDPOINT_ID) {
        fprintf(stderr, "Endpoint %u is out of range (0-%u)\n", endpoint, NFM_MAX_ENDPOINT_ID);
        return 1;
      }
      break;
    case 'm':
      if (strspn(optarg, "0123456789x:abcdefABCDEF+nfe.")!=strlen(optarg)) {
        print_usage(argv[0]);
      }
      opt=malloc(strlen(optarg)+1);
      strcpy(opt, optarg);
      tok=strtok(opt,":");
      while (tok) {
        unsigned int _card,_endpoint,_id;
        if (!strchr(tok, '.')) {
          print_usage(argv[0]);
        }
        num_multi++;
        multi=realloc(multi, sizeof(unsigned int)*num_multi);
        errno=0;
        while (*tok=='+' || *tok=='n' || *tok=='f' || *tok=='e')
          ++tok;
        _card=strtoul(tok, 0, 0);
        if (errno==EINVAL) {
          print_usage(argv[0]);
        }
        tok=strchr(tok, '.');
        if (!tok) {
          print_usage(argv[0]);
        }
        ++tok;
        while (*tok=='+' || *tok=='n' || *tok=='f' || *tok=='e')
          ++tok;
        _endpoint=strtoul(tok, 0, 0);
        if (errno==EINVAL) {
          print_usage(argv[0]);
        }
        tok=strchr(tok, '.');
        if (!tok) {
          print_usage(argv[0]);
        }
        ++tok;
        while (*tok=='+' || *tok=='n' || *tok=='f' || *tok=='e')
          ++tok;
        _id=strtoul(tok, 0, 0);
        multi[num_multi-1]=NFM_CARD_ENDPOINT_ID(_card, _endpoint, _id);
        tok=strtok(0, ":");
      }
      if (num_multi==0) {
        print_usage(argv[0]);
      }
      free(opt); opt=0;
      break;
    case 'x':
      enable_eof=(unsigned int)strtoul(optarg,0,0);
      change_eof=1;
      break;
    case 'c':
      context=(unsigned int)strtoul(optarg,0,0);
      change_context=1;
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc)
    print_usage(argv[0]);

  if (opt) {
    free(opt); opt=0;
  }

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = sig_term;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);

  if (num_multi==0) {
    ns_packet_extra_options_t opt;
    printf("Opening device %u endpoint %u ID %u\n", device, endpoint, host_id);
    memset(&opt, 0, sizeof(opt));
    opt.sof_fptr=sof_callback;
    opt.eof_fptr=eof_callback;
    opt.host_inline=1;
    if (NS_NFM_SUCCESS != (r = ns_packet_open_device_ex(&dev, NFM_CARD_ENDPOINT_ID(device, endpoint, host_id), &opt))) {
      print_error(r, "ns_packet_open_device");
      return -1;
    }
  } else {
    ns_packet_extra_options_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.sof_fptr=sof_callback;
    opt.eof_fptr=eof_callback;
    opt.host_inline=1;
    printf("Opening multiple: ");
    for(index=0; index<num_multi; ++index) {
      printf("%sdevice=%u endpoint=%u ID=%u", (index==0)?"":",", NFM_CARD_FROM_BITFIELD(multi[index]), NFM_ENDPOINT_FROM_BITFIELD(multi[index]), NFM_HOSTID_FROM_BITFIELD(multi[index]));
    }
    printf("\n");
    if (NS_NFM_SUCCESS != (r = ns_packet_open_multi_device_ex(&dev, multi, num_multi, &opt))) {
      print_error(r, "ns_packet_open_device");
      return -1;
    }
    free(multi);
  }

  while (running) {
    if (NS_NFM_SUCCESS != (r = ns_packet_receive(dev, &pckt, flags))) {
      if (running)
        print_error(r, "ns_packet_receive");
      continue;
    }

    ns_packet_transmit(dev, &pckt, 0);
  }

  ns_packet_close_device(dev);

  return 0;
}
