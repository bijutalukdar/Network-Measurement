/**
 * Copyright (C) 2004-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_packet_flow_modify.c
 * Description: Sample wire application using the packet API, and the packet
 *              handle flow modification API demonstrating how to reject HTTP
 *              traffic.
 */

#include "ns_packet.h"
#include "ns_flow.h"
#include "ns_log.h"

#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define print_error(r, prefix)  fprintf(stderr, "%s: %s: %s. (subcode=%d).\n", prefix, ns_nfm_module_string(r), ns_nfm_error_string(r), NS_NFM_ERROR_SUBCODE(r))

volatile int running = 1;

void sig_term(int __attribute__((unused)) dummy)
{
  running = 0;
}

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  " -e --endpoint n Select endpoint (default 1)\n"
                  " -i --host_id i  Set host destination id (default 15)\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"host_id",   1, 0, 'i'},
  {"endpoint",  1, 0, 'e'},
  {"device",    1, 0, 'd'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

// return 1 if the flow was not rejected, 0 otherwise
int process_packet(ns_packet_t* pckt) {
  ns_nfm_ret_t ret;
  ns_flow_modifier_h fm;
  // look for TCP with dest port 80, and reject flow
  if (pckt->packet_length >= (ETH_HLEN+20+20)) {
    // packet greater than minimum size TCP packet
    struct iphdr* ip=(struct iphdr*)(pckt->packet_data+14);
    if (ip->version==4 && ip->protocol==IPPROTO_TCP) {
      // likely a TCP packet
      struct tcphdr *tcp=(struct tcphdr *)((char *)(ip)+(ip->ihl*4));
      if (ntohs(tcp->dest)==80) {
        // TCP destination port == 80 (HTTP), reject flow
        printf("rejecting HTTP flow\n");
        ret=ns_flow_create_packet_handle_modifier(pckt, &fm);
        if (ret==NS_NFM_SUCCESS) {
          ns_flow_reject(fm);
          ns_flow_apply_modifier(fm);
          return 0;
        } else {
          print_error(ret, "ns_flow_create_packet_handle_modifier");
        }
      }
    }
  }
  return 1;
}

int main(int argc, char *argv[])
{
  ns_nfm_ret_t r;
  ns_packet_device_h dev;
  ns_packet_t pckt;
  unsigned int host_id = 15;
  unsigned int device = 0;
  unsigned int endpoint = 1;
  ns_packet_extra_options_t opt;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:hi:d:e:", __long_options, NULL)) != -1) {
    switch (c) {
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
    case 'e':
      endpoint = (unsigned int)strtoul(optarg,0,0);
      if (endpoint > NFM_MAX_ENDPOINT_ID) {
        fprintf(stderr, "Endpoint %u is out of range (0-%u)\n", endpoint, NFM_MAX_ENDPOINT_ID);
        return 1;
      }
      break;
    case 'd':
      device = (unsigned int)strtoul(optarg,0,0);
      if (device > 3) {
        fprintf(stderr, "Device %d is out of range (0-3)\n", device);
        exit(1);
      }
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc)
    print_usage(argv[0]);

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = sig_term;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);

  memset(&opt, 0, sizeof(opt));
  opt.host_inline=1;
  if (NS_NFM_SUCCESS != (r = ns_packet_open_device_ex(&dev, NFM_CARD_ENDPOINT_ID(device, endpoint, host_id), &opt))) {
    print_error(r, "ns_packet_open_device");
    return -1;
  }

  while (running) {
    if (NS_NFM_SUCCESS != (r = ns_packet_receive(dev, &pckt, 0))) {
      if (running) {
        print_error(r, "ns_packet_receive");
      }
      continue;
    }

    if (process_packet(&pckt)) {
      if (NS_NFM_SUCCESS != (r = ns_packet_transmit(dev, &pckt, 0))) {
        print_error(r, "ns_packet_transmit");
        ns_packet_destroy(&pckt);
      }
    } else { // flow was rejected, discard this packet
      ns_packet_destroy(&pckt);
    }
  }

  ns_packet_close_device(dev);
  return 0;
}
