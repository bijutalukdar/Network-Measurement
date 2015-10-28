/**
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 * File:        nfm_sample_indtbl.c
 * Description: Sample application for the indirect table API.
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
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  " -i --host_id i  Set host destination id (default 15)\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"host_id",   1, 0, 'i'},
  {"device",    1, 0, 'd'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

int main(int argc, char* argv[])
{
  ns_nfm_ret_t ret;
  unsigned int host_id = 15;
  unsigned int device = 0;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:hi:d:", __long_options, NULL)) != -1) {
    switch (c) {
    case 'l':
      ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
      break;
    case 'i':
      host_id = (unsigned int)strtoul(optarg,0,0);
      if (host_id > 31) {
        fprintf(stderr, "Host_id %d is out of range (0-31)\n", device);
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
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc)
    print_usage(argv[0]);

  atexit(atexit_func);

  // Make sure we fail right on a non-exiting device
  unsigned int nodevice = 10;
  printf("Initializing messaging on an non existing cardid %d\n", nodevice);
  ret = ns_indtbl_init_messaging(&h, nodevice);
  if (NS_NFM_ERROR_CODE(ret) == NS_NFM_SUCCESS) {
    fprintf(stderr, "error: should not init card %d\n", nodevice);
    exit(1);
  }

  // Initialize NFM messaging
  printf("Initializing messaging...");
  ret = ns_indtbl_init_messaging(&h, device);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
    fprintf(stderr, " failed\n");
    exit(1);
  }
  printf(" done\n");

  printf("Initializing VLAN table...");
  ret = ns_indtbl_l2_init_table(h);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
    fprintf(stderr, " failed\n");
    exit(1);
  }
  printf(" done\n");


  // Set many entries for vlanid

  unsigned int override_fst = 0;
  unsigned int port, min_port, max_port;
  nfm_get_hw_min_port(&min_port);
  nfm_get_hw_max_port(&max_port);

  for (port = min_port; port <= max_port; port++) {
    override_fst = (port < 2) ? 0 : 1;
    printf("Setting override_fst %u for port %u, all vlans ...",override_fst, port);
    ret = ns_indtbl_l2_set_override_fst(h, port, NS_VLAN_ID_ALL, override_fst);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
      fprintf(stderr, " failed\n");
      exit(1);
    }
    printf(" done\n");
  }

  return 0;
}
