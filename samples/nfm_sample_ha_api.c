/*
 * Copyright (C) 2012 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_ha_api.c
 * Description: Sample application using NFM high availability API to verify
 *              and configure HA mode on a box
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "ns_log.h"
#include "nfm_error.h"
#include "ns_ha.h"

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -p n  Set vnic pif\n"
                  " -s Display HA configuration\n"
                  " -u specify unit ID target\n"
                  " -a specify applicance ID target\n"
                  " -i specify new IPv6 address for target\n"
                  " -v verify connectivity to target\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",      required_argument, 0, 'p'},
  {"status",        no_argument,       0, 's'},
  {"unit_id",       required_argument, 0, 'u'},
  {"appliance_id",  required_argument, 0, 'a'},
  {"ip_addr",       required_argument, 0, 'i'},
  {0, 0, 0, 0}
};

#define MAX_NFE         4

#define MAX_ENTRIES     1024*4
#define ENTRY_SIZE      16
#define BUF_SIZE        (ENTRY_SIZE*MAX_ENTRIES)
#define BITS_PER_BYTE   8

int main(int argc, char *argv[])
{
  ns_nfm_ret_t ret;
  unsigned int set_vnic_pif = 0;
  unsigned int vnic_pif = 0;
  unsigned int display_config = 0;
  int uid = -1;
  int aid = -1;
  unsigned int set_ip = 0;
  struct in6_addr ip;
  unsigned int verify_connected = 0;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "p:vshu:a:i:", __long_options, NULL)) != -1) {
    switch (c) {
    case 'l':
      ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
      break;
    case 'p':
        set_vnic_pif = 1;
        vnic_pif = (unsigned int)strtoul(optarg,0,0);
      break;
    case 'u':
        uid = (unsigned int)strtoul(optarg,0,0);
        break;
    case 's':
        display_config = 1;
        break;
    case 'a':
        aid = (int)strtol(optarg,0,0);
        break;
    case 'i':
        if (!inet_pton(AF_INET6, optarg, &ip)) {
            fprintf(stderr, "Bad ipv6 address: %s \n",strerror(errno));
            return -1;
        }
        set_ip = 1;
      break;
    case 'v':
        verify_connected = 1;
        break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc || argc == 1)
    print_usage(argv[0]);

  if (set_vnic_pif) {
      ret = ns_ha_set_vnic_pif(vnic_pif);
      if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "Failed to set VNIC pif\n");
        return -1;
      }
  }
  
  if (display_config) {
      int cuid, caid;
      ret = ns_ha_get_config(&cuid, &caid);
      if (ret == NS_NFM_SUCCESS) {
          printf("HA Configuration: UID=%i AID=%i\n", cuid, caid);
      } else {
        fprintf(stderr, "Failed to get HA configuration\n");
        return -1;
      }
  }

  if (verify_connected) {
      if (uid == -1 || aid == -1) {
          fprintf(stderr, "UID or AID unspecified.\n");
          return -1;
      }
      int is_connected = 0;
      ret =  ns_ha_verify_connectivity(uid, aid, &is_connected);
      if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "Failed to verify connectivity.\n");
        return -1;
      }

      if (is_connected)
          printf("Connected.\n");
      else
          printf("Not connected.\n");

  }

  if (set_ip) {
      if (uid == -1 || aid == -1) {
          fprintf(stderr, "UID or AID unspecified.\n");
          return -1;
      }
      ret = ns_ha_set_vnic_ip_addr(uid,aid, &ip);
      if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "Failed to set VNIC IPv6 address\n");
        return -1;
      }
  }

  return 0;
}

