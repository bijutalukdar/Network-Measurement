/**
 * Copyright (C) 2010-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_dump_ifids.c
 * Description: Sample application to display the L2 (interface ID) table.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>

#include "ns_indtbl.h"
#include "ns_log.h"
#include "nfm_platform.h"

#define CKRET(ret, msg)                         \
  if (NS_NFM_ERROR_CODE(ret)!=NS_NFM_SUCCESS) { \
    NS_LOG_ERROR(msg " (%d,%d): %s",             \
            NS_NFM_ERROR_CODE(ret),             \
            NS_NFM_ERROR_SUBCODE(ret),          \
            ns_nfm_error_string(ret));          \
    return -1;                                  \
  }

static ns_indtbl_h h = 0;

static void atexit_func(int s __attribute__ ((unused)))
{
  if (h) {
    printf("shutdown_messaging...\n");
    ns_indtbl_shutdown_messaging(h);
  }
  exit(-1);
}

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options] \n"
                  "\n"
                  "Will print out a table containing External port, VLAN ID, Interface ID.\n"
                  "The asterisk notation means all values up to the next non-asterisk are\n"
                  "the same as the value immediately above it (and up to max 4096 VLAN ID).\n\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  "\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"device",    1, 0, 'd'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

int main(int argc, char* argv[])
{
  ns_nfm_ret_t ret;
  unsigned int device = 0;
  unsigned int port, intport;
  unsigned int ifid = 0xFFFFFFFF;
  unsigned int vlan = NS_VLAN_ID_ALL;
  unsigned int min, max, nnfe, lastifid, firstdup;
  unsigned int ifids[4096];

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
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc)
    print_usage(argv[0]);

  signal(SIGINT, atexit_func);
  signal(SIGTERM, atexit_func);

  ret = nfm_get_nfes_present(&nnfe);
  CKRET(ret, "Error getting # of NFEs present");
  printf("%u NFEs present\n", nnfe);

  // Enumerate the ports
  ret = nfm_get_external_min_port(&min);
  CKRET(ret, "Error getting external minimum port");
  ret = nfm_get_external_max_port(&max);
  CKRET(ret, "Error getting external maximum port");

  // Initialize NFM messaging
  ret = ns_indtbl_init_messaging(&h, device);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
    fprintf(stderr, "Failed to initialize messaging\n");
    exit(1);
  }
  printf("Reading device %d...\n", device);

  printf("%20s, %15s, %15s\n", "External port", "VLAN ID", "Interface ID");
  for (port = min; port <= max; port++) {
    vlan = 0;
    nfm_get_internal_port_from_external(port, &intport);
    ret = ns_indtbl_l2_dump_ifids(h, intport, ifids);
    CKRET(ret, "Error in ns_indtbl_l2_dump_ifids");
    printf("%20d, %15s, %15d\n", port, "--", ifids[0]);
    lastifid=256;
    firstdup=0;
    // Print out ifid for all ports, and all VLAN ID.
    //   use * to notate values which are the same as above
    for (vlan=1; vlan<4096; vlan++) {
      ifid=ifids[vlan];
      if(lastifid != ifid) {
        printf("%20s, %15d, %15d\n", "*", vlan, ifid);
        firstdup=0;
      }
      else if(lastifid == ifid && !firstdup) {
        printf("%20s, %15s, %15s\n", "*", "*", "*");
        firstdup=1;
      }
      lastifid=ifid;
    }
  }
  ns_indtbl_shutdown_messaging(h);
  return 0;
}
