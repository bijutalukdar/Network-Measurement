/**
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "ns_indtbl.h"
#include "ns_log.h"

static ns_indtbl_h h = 0;

static void atexit_func(void)
{
  if (h)
    ns_indtbl_shutdown_messaging(h);
}

static void set_bump_port(unsigned int port, unsigned int vlan,
                          unsigned int bump_port)
{
  ns_nfm_ret_t ret;
  printf("Setting bump port to %u for port %u, for vlan %u...", bump_port, port, vlan);
  ret = ns_indtbl_l2_set_bump_port(h, port, vlan, bump_port);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
    printf(" failed\n");
    fflush(stdout);
    exit(1);
  }
  printf(" done\n");
}


static void get_bump_port(unsigned int port, unsigned int vlan,
                          unsigned int *bump_port)
{
  ns_nfm_ret_t ret;
  printf("Getting bump port for port %u, vlan %u ...", port, vlan);
  ret = ns_indtbl_l2_get_bump_port(h, port, vlan, bump_port);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
    printf(" failed\n");
    fflush(stdout);
    exit(1);
  }
  printf(" done\n");
}


static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options] <port> [<bump port>]\n"
                  "\n"
                  "If the bump port isn't specified, the utility gets the \n"
                  "current bump port for <port>.  Otherwise it sets the bump\n"
                  "port for <port> to <bump port>\n"
                  "\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  " -v --vlan n     Select the VLAN to set (default ALL)\n"
                  " -b --bidir      Set bump port in both directions\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"device",    1, 0, 'd'},
  {"vlan",      1, 0, 'v'},
  {"bidir",     0, 0, 'b'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

int main(int argc, char* argv[])
{
  ns_nfm_ret_t ret;
  unsigned int device = 0;
  unsigned int port, bump_port;
  unsigned int bidirectional = 0;
  unsigned int vlan = NS_VLAN_ID_ALL;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:d:v:bh", __long_options, NULL)) != -1) {
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
      if (vlan > 4095 && vlan != NS_VLAN_ID_ALL) {
        fprintf(stderr, "VLAN ID %d is out of range (0-4095 or %u for ALL)\n",
                vlan, NS_VLAN_ID_ALL);
        exit(1);
      }
      break;
    case 'b':
      bidirectional = 1;
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if ((optind < argc - 2) || (optind > argc - 1))
    print_usage(argv[0]);
  port = (unsigned int)strtoul(argv[optind],0,0);

  atexit(atexit_func);

  /* Initialize NFM messaging */
  printf("Initializing messaging...");
  ret = ns_indtbl_init_messaging(&h, device);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
    fprintf(stderr, " failed\n");
    exit(1);
  }
  printf(" done\n");

  /* Set the bump port for port 0 to 1 and visa versa */
  if (optind <= argc - 2) {
    bump_port = (unsigned int)strtoul(argv[optind+1],0,0);
    set_bump_port(port, vlan, bump_port);
    if (bidirectional)
      set_bump_port(bump_port, vlan, port);
  } else {
    if (vlan == NS_VLAN_ID_ALL)
      vlan = 0;
    get_bump_port(port, vlan, &bump_port);
    printf("Bump port for %u vlan %u is %u\n", port, vlan, bump_port);
  }

  return 0;
}
