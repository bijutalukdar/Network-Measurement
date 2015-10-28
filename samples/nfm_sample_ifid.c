/**
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_ifid.c
 * Description: Sample application to demonstrate the interface ID API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "ns_indtbl.h"
#include "ns_log.h"
#include "nfm_platform.h"

static ns_indtbl_h h = 0;

static void atexit_func(void)
{
  if (h)
    ns_indtbl_shutdown_messaging(h);
}

static void print_usage(const char* argv0)
{
  unsigned int max;
  nfm_get_external_max_port(&max);
  nfm_get_internal_port_from_external(max, &max);
  fprintf(stderr, "USAGE: %s [options] <intport> [<ifid>]\n"
                  "\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  " -v --vlan n     Select VLAN ID (default: ALL)\n"
                  "\n"
                  "<intport> must be in [0,%u] and <ifid> must be in [0,255]\n",
          argv0, max);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"device",    1, 0, 'd'},
  {"vlan",      1, 0, 'v'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

int main(int argc, char* argv[])
{
  ns_nfm_ret_t ret;
  unsigned int device = 0;
  unsigned int port;
  unsigned int ifid = 0xFFFFFFFF;
  unsigned int vlan = NS_VLAN_ID_ALL;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:hd:v:", __long_options, NULL)) != -1) {
    switch (c) {
    case 'l':
      ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
      break;
    case 'd':
      device = (unsigned int)strtoul(optarg,0,0);
      if (device > 3) {
        fprintf(stderr, "Device %d is out of range (0-3)\n", device);
        exit(1);
      }
      break;
    case 'v':
      vlan = (unsigned int)strtoul(optarg,0,0);
      if (vlan > 4095) {
        fprintf(stderr, "VLAN ID %d is out of range (0-4095)\n", vlan);
        exit(1);
      }
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if ((optind < argc - 2) || (optind > argc - 1))
    print_usage(argv[0]);

  port = atoi(argv[optind]);

  if (optind < argc - 1) {
    ifid = atoi(argv[optind+1]);
    if (ifid > 255)
      print_usage(argv[0]);
  }

  atexit(atexit_func);

  // Initialize NFM messaging
  ret = ns_indtbl_init_messaging(&h, device);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
    fprintf(stderr, "Failed to initialize messaging\n");
    exit(1);
  }

  if (ifid != 0xFFFFFFFF) {
    ret = ns_indtbl_l2_set_ifid(h, port, vlan, ifid);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
      fprintf(stderr, "Failed to set interface ID\n");
      exit(1);
    }
    if (vlan == NS_VLAN_ID_ALL)
      printf("Set port %u to interface ID %u for all VLANS\n", port,
             ifid);
    else
      printf("Set port %u VLAN %u to interface ID %u\n", port,
             vlan, ifid);
  }
  else {
    if (vlan == NS_VLAN_ID_ALL)
      vlan = 0;
    ret = ns_indtbl_l2_get_ifid(h, port, vlan, &ifid);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
      fprintf(stderr, "Failed to get interface ID\n");
      exit(1);
    }
    printf("Port %u on VLAN %u has interface ID %u\n", port,
           vlan, ifid);
  }

  return 0;
}
