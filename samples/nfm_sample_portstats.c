/**
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_portstats.c
 * Description: NFM sample application that gathers port statistics
 */

#include "ns_log.h"
#include "nfe_interface.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PORTSTAT(stat_name) (unsigned long long)stats->port[p].stat_name

static void print_stats(unsigned card, const nfm_portstats_t* stats)
{
  unsigned int p;
  for (p = 0; p < NFE_MAX_PORTS; p++) {
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] OctetsTotalOK RX: %llu\n", card, p, PORTSTAT(rx.octets_total_OK));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] OctetsBad RX: %llu\n", card, p, PORTSTAT(rx.octets_bad));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] UnicastPackets RX: %llu\n", card, p, PORTSTAT(rx.unicast_packets));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] MulticastPackets RX: %llu\n", card, p, PORTSTAT(rx.multicast_packets));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] BroadcastPackets RX: %llu\n", card, p, PORTSTAT(rx.broadcast_packets));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets64Octets RX: %llu\n", card, p, PORTSTAT(rx.packets_64));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets65to127Octets RX: %llu\n", card, p, PORTSTAT(rx.packets_65_to_127));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets128to255Octets RX: %llu\n", card, p, PORTSTAT(rx.packets_128_to_255));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets256to511Octets RX: %llu\n", card, p, PORTSTAT(rx.packets_256_to_511));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets512to1023Octets RX: %llu\n", card, p, PORTSTAT(rx.packets_512_to_1023));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets1024to1518Octets RX: %llu\n", card, p, PORTSTAT(rx.packets_1024_to_1518));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets1519toMaxOctets RX: %llu\n", card, p, PORTSTAT(rx.packets_1519_to_max));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] OctetsTotalOK TX: %llu\n", card, p, PORTSTAT(tx.octets_total_OK));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] OctetsBad TX: %llu\n", card, p, PORTSTAT(tx.octets_bad));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] UnicastPackets TX: %llu\n", card, p, PORTSTAT(tx.unicast_packets));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] MulticastPackets TX: %llu\n", card, p, PORTSTAT(tx.multicast_packets));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] BroadcastPackets TX: %llu\n", card, p, PORTSTAT(tx.broadcast_packets));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets64Octets TX: %llu\n", card, p, PORTSTAT(tx.packets_64));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets65to127Octets TX: %llu\n", card, p, PORTSTAT(tx.packets_65_to_127));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets128to255Octets TX: %llu\n", card, p, PORTSTAT(tx.packets_128_to_255));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets256to511Octets TX: %llu\n", card, p, PORTSTAT(tx.packets_256_to_511));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets512to1023Octets TX: %llu\n", card, p, PORTSTAT(tx.packets_512_to_1023));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets1024to1518Octets TX: %llu\n", card, p, PORTSTAT(tx.packets_1024_to_1518));
    NS_LOG_RAW(NS_LOG_LVL_INFO, "[%d,%d] Packets1519toMaxOctets TX: %llu\n", card, p, PORTSTAT(tx.packets_1519_to_max));
  }
}


void usage(char *name)
{
  fprintf(stderr, "usage: %s [-c card]\n\n", name);
  exit(1);
}


int main(int argc, char *argv[])
{
  ns_nfm_ret_t ret;
  nfm_portstats_t stats;
  unsigned card = 0;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  if (argc > 1) {
    if (argc != 3 || strcmp(argv[1], "-c") != 0)
      usage(argv[0]);
    card = atoi(argv[2]);
  }

  if ((ret = nfe_interface_get_portstats(card, &stats)) != 0) {
    NS_LOG_ERROR("Error getting portstats: quitting\n");
    return 1;
  }

  print_stats(card, &stats);

  return 0;
}
