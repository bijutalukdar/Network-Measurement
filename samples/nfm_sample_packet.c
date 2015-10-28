/**
 * Copyright (C) 2004-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_packet.c
 * Description: Sample wire application using the packet API.
 */

#include "ns_packet.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/time.h>
#include <errno.h>

#include "ns_log.h"

unsigned long long numbytes = 0;
unsigned long long numpkts = 0;
pid_t pid = 0;

static void dump(unsigned char *data, unsigned int len)
{
  unsigned int i;
  for (i = 0; i < len; i++) {
    if (i%16 == 0)
      printf("\n");
    else if (i%2 == 0)
      printf(" ");
    printf("%02x", data[i]);
  }
  printf("\n\n");
}

#define print_error(r, prefix)  fprintf(stderr, "%s: %s: %s. (subcode=%d).\n", prefix, ns_nfm_module_string(r), ns_nfm_error_string(r), NS_NFM_ERROR_SUBCODE(r))

volatile int running = 1;

void sig_term(int __attribute__((unused)) dummy)
{
  running = 0;
}

void sig_usr1(int __attribute__((unused)) dummy)
{
  printf("%8d  %16llu  %16llu\n", pid, numpkts, numbytes);
  fflush(stdout);
}

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  " -e --endpoint n Select endpoint (default 1)\n"
                  " -i --host_id i  Set host destination id (default 15)\n"
                  " -D --drop       Drop all received packets (default: transmit all received packets)\n"
                  " -a --arp        Receive all ARPs\n"
                  " -m --multi d.e.i[:d.e.i]... Receive from multiple device.endpoint.id tuples (ignore -d, -e and -i options)\n"
                  " -# --counters   Show packet counters at program exit\n"
                  " -A --adaptive   Use adaptive polling (decrease latency at the cost of some CPU while no traffic)\n"
                  " -L --load       Enable artificial processing of packet contents to generate memory/CPU load\n"
                  " -p --print N    Print stats at every N packets (N >= 1: default 300000) at EXTRA log level (-l 6)\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"host_id",   1, 0, 'i'},
  {"endpoint",  1, 0, 'e'},
  {"device",    1, 0, 'd'},
  {"multi",     1, 0, 'm'},
  {"drop",      0, 0, 'D'},
  {"arp",       0, 0, 'a'},
  {"print",     1, 0, 'p'},
  {"help",      0, 0, 'h'},
  {"counters",  0, 0, '#'},
  {"adaptive",  0, 0, 'A'},
  {"load",      2, 0, 'L'},
  {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
  ns_nfm_ret_t r;
  ns_packet_device_h dev;
  ns_packet_t pckt;
  unsigned int host_id = 15;
  unsigned int endpoint = 1;
  unsigned int device = 0;
  unsigned int flags = 0;
  int drop_all = 0;
  int request_arp = 0;
  unsigned int print_packets = 300000;
  struct timeval start, end;
  uint64_t start_us, end_us;
  ns_log_lvl_e loglevel;
  unsigned int* multi=0;
  unsigned int num_multi=0;
  unsigned int index;
  char* opt=0;
  char* tok=0;
  unsigned int show_counters=0;
  unsigned int adaptive_poll=0;
  unsigned int enable_load=0;

  pid = getpid();
  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:hi:d:e:Dp:m:L:a#A::", __long_options, NULL)) != -1) {
    switch (c) {
    case '#':
      show_counters=1;
      break;
    case 'L':
      enable_load=1;
      if (optarg) {
        enable_load = (unsigned int)strtoul(optarg,0,0);
      }
      break;
    case 'A':
      adaptive_poll=100; // 100 microseconds
      flags|=NS_PACKET_RECEIVE_ADAPTIVE_POLL;
      break;
    case 'l':
      ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
      break;
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
    case 'D':
      drop_all = 1;
      break;
    case 'a':
      request_arp=1;
      break;
    case 'p':
      print_packets = (unsigned int)strtoul(optarg,0,0);
      if (print_packets == 0) {
        fprintf(stderr, "Use a sensible value for print_packets, (not %d)\n", print_packets);
        exit(1);
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
  action.sa_handler = sig_usr1;
  sigaction(SIGUSR1, &action, NULL);

  if (num_multi==0) {
    ns_packet_extra_options_t opt;
    printf("Opening device %u endpoint %u ID %u\n", device, endpoint, host_id);
    memset(&opt, 0, sizeof(opt));
    opt.host_inline=1-drop_all;
    opt.request_arp=request_arp;
    opt.adaptive_poll_us=adaptive_poll;
    if (NS_NFM_SUCCESS != (r = ns_packet_open_device_ex(&dev, NFM_CARD_ENDPOINT_ID(device, endpoint, host_id), &opt))) {
      print_error(r, "ns_packet_open_device");
      return -1;
    }
  } else {
    ns_packet_extra_options_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.host_inline=1-drop_all;
    opt.request_arp=request_arp;
    opt.adaptive_poll_us=adaptive_poll;
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
  if (show_counters) {
    ns_packet_enable_counters(dev);
  }

  ns_log_lvl_get(&loglevel);

  while (running) {
    if (NS_NFM_SUCCESS != (r = ns_packet_receive(dev, &pckt, flags))) {
      if (running)
        print_error(r, "ns_packet_receive");
      continue;
    }

    NS_LOG_VERBOSE("Dumping packet data\n");
    if (loglevel == NS_LOG_LVL_VERBOSE) {
      dump(pckt.packet_data, pckt.packet_length);
    }

    NS_LOG_VERBOSE("%lu.%6lu Received packet with length %d from lif%d to lif%d\n",
                   pckt.timestamp_s, pckt.timestamp_us, pckt.packet_length,
                   (int)ns_packet_get_ingress_logical_interface_id(&pckt),
                   (int)ns_packet_get_egress_logical_interface_id(&pckt));

    numpkts++;
    if (numpkts == 1) {
      gettimeofday(&start, NULL);
      start_us = start.tv_sec * 1000000 + start.tv_usec;
    }

    numbytes += pckt.packet_length;

    if (numpkts%print_packets == 0) {
      gettimeofday(&end, NULL);
      end_us = end.tv_sec * 1000000 + end.tv_usec;
      unsigned int us = end_us - start_us;
      double mbps = (numbytes*8.0)/(1.0*us);
      NS_LOG_EXTRA("numpkts %llu numbytes %llu Mbps %f", numpkts, numbytes, mbps);
    }

    if (enable_load && pckt.packet_data && pckt.packet_length>=4) {
      unsigned int iter;
      for (iter=0; iter<enable_load; ++iter) {
        unsigned int* pc=(unsigned int*)pckt.packet_data;
        unsigned int count=pckt.packet_length>>2;
        if (pckt.packet_length>=64) {
          count=16;
        }
        while (count--) {
          *pc=ntohl(*pc);
        }
        pc=(unsigned int*)pckt.packet_data;
        if (pckt.packet_length>=64) {
          count=16;
        } else {
          count=pckt.packet_length>>2;
        }
        while (count--) {
          *pc=htonl(*pc);
        }
      }
    }

    if (drop_all) {
      ns_packet_destroy(&pckt);
    } else {
      if (NS_NFM_SUCCESS != (r = ns_packet_transmit(dev, &pckt, 0))) {
        print_error(r, "ns_packet_transmit");
        ns_packet_destroy(&pckt);
      }
    }
  }

  if (show_counters) {
    struct ns_packet_counters_t c;
    if (ns_packet_get_counters(dev, &c)==NS_NFM_SUCCESS) {
      printf("Packet counters\nrecv_count=%llu, recv_no_msg=%llu, recv_msg=%llu, recv_block=%llu, send_count=%llu, send_block=%llu, send_no_msg=%llu, free_count=%llu, free_block=%llu, alloc_count=%llu, alloc_syscall=%llu\n", (unsigned long long)c.recv_count, (unsigned long long)c.recv_no_msg, (unsigned long long)c.recv_msg, (unsigned long long)c.recv_block, (unsigned long long)c.send_count, (unsigned long long)c.send_block, (unsigned long long)c.send_no_msg, (unsigned long long)c.free_count, (unsigned long long)c.free_block, (unsigned long long)c.alloc_count, (unsigned long long)c.alloc_syscall);
    }
  }

  ns_packet_close_device(dev);

  return 0;
}
