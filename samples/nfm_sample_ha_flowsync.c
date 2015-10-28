/*
 * Copyright (C) 2012 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_ha_flowsync.c
 * Description: Sample application using NFM high availability flow API to
 *              synchronize flow state to the opposite box
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "ns_flow.h"
#include "ns_flow_ha.h"
#include "ns_log.h"
#include "ns_packet.h"
#include "nfm_error.h"

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -a --addr_space  Filter FST with addr space\n"
                  " -c --clearmask   Clear the syncmask\n"
                  " -f --flowmask    Fetch the flowmask\n"
                  " -l --loglevel n  Set log level\n"
                  " -r --remote_req  The request be performed for the\n"
                  "                  opposing box over the HA link.\n"
                  "                  (not compatible with `-t' flag)\n"
                  " -s --syncmask    Fetch the syncmask\n"
                  " -t --flowsync    Sync all active flows\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"addr_space",1, 0, 'a'},
  {"loglevel",  1, 0, 'l'},
  {"remote_req",1, 0, 'r'},
  {"flowmask",  0, 0, 'f'},
  {"syncmask",  0, 0, 's'},
  {"clearmask", 0, 0, 'c'},
  {"flowsync",  0, 0, 't'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

#define MAX_NFE         4

#define MAX_ENTRIES     1024*1024*4
#define BITS_PER_BYTE   8
#define BUF_SIZE        ((MAX_ENTRIES+(BITS_PER_BYTE-1))/BITS_PER_BYTE)

int main(int argc, char *argv[])
{
  ns_flow_ha_h *h;
  ns_nfm_ret_t ret;
  unsigned char *buf;
  unsigned int remote_req = 0;
  unsigned int flowmask = 0;
  unsigned int syncmask = 0;
  unsigned int flowsync = 0;
  unsigned int clearmask = 0;
  int as = -1; // address space
  uint64_t size = BUF_SIZE;
  uint32_t flows, i, nfe;

  ns_packet_device_h dev;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "a:l:h:rfstc", __long_options, NULL)) != -1) {
    switch (c) {
    case 'a':
      as = strtol(optarg,0,0);
      break;
    case 'l':
      ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
      break;
    case 'r':
      remote_req = 1;
      break;
    case 'f':
      flowmask = 1;
      break;
    case 's':
      syncmask = 1;
      break;
    case 'c':
      clearmask = 1;
      break;
    case 't':
      flowsync = 1;
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc || argc == 1)
    print_usage(argv[0]);

  buf = calloc(BUF_SIZE, sizeof(unsigned char));
  if (buf == NULL) {
      fprintf(stderr, "Failed to allocate memory\n");
      exit(1);
  }

  ret = ns_flow_ha_open(&h);
  if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "Failed to open flow HA handle\n");
      free(buf);
      exit(1);
  }

  if (flowmask) {
    for (nfe = 0; nfe < MAX_NFE; nfe++) {
      bzero(buf, BUF_SIZE * sizeof(unsigned char));
      if (remote_req)
          ret = ns_flow_ha_get_remote_flowmask(h, nfe, as, buf, &size, &flows);
      else
          ret = ns_flow_ha_get_flowmask(h, nfe, as, buf, &size, &flows);

      if (ret != NS_NFM_SUCCESS)
        continue;

      if (as != -1)
        printf("%s%s NFE%u: address space %d, %u flows:\n", (nfe)?"\n":"",
               (remote_req)?"remote":"local", nfe, as, flows);
      else
        printf("%s%s NFE%u: %u flows:\n", (nfe)?"\n":"",
               (remote_req)?"remote":"local", nfe, flows);
      for (i = 0; i < (size * BITS_PER_BYTE); i++) {
        if (buf[i / BITS_PER_BYTE] & (0x1 << (i % BITS_PER_BYTE)))
          printf("NFE%u: Flow ID %u active\n", nfe, i);
      }
    }
  }

  if (syncmask) {
    for (nfe = 0; nfe < MAX_NFE; nfe++) {
      bzero(buf, BUF_SIZE * sizeof(unsigned char));
      if (remote_req)
          ret = ns_flow_ha_get_remote_syncmask(h, nfe, buf, &size);
      else
          ret = ns_flow_ha_get_syncmask(h, nfe, buf, &size);
      if (ret != NS_NFM_SUCCESS)
        continue;

      printf("%s%s NFE%u:\n", (nfe)?"\n":"",(remote_req)?"remote":"local", nfe);
      for (i = 0; i < (size * BITS_PER_BYTE); i++) {
        if (buf[i / BITS_PER_BYTE] & (0x1 << (i % BITS_PER_BYTE)))
          printf("NFE%u: Flow ID %u synced\n", nfe, i);
      }
    }
  }

  if (flowsync) {
    for (nfe = 0; nfe < MAX_NFE; nfe++) {
      bzero(buf, BUF_SIZE * sizeof(unsigned char));
      ret = ns_flow_ha_get_flowmask(h, nfe, as, buf, &size, &flows);
      if (ret != NS_NFM_SUCCESS)
        continue;

      if (as != -1)
        printf("%slocal NFE%u address space %d:\n", (nfe)?"\n":"", nfe, as);
      else
        printf("%slocal NFE%u:\n", (nfe)?"\n":"", nfe);
      ret = ns_packet_open_device(&dev, NFM_CARD_ENDPOINT_ID(nfe, 1, NFM_ANY_ID));
      if (ret!=NS_NFM_SUCCESS) {
        fprintf(stderr, "Failed to open packet device\n");
        continue;
      }

      for (i = 0; i < (size * BITS_PER_BYTE); i++) {
        if (buf[i / BITS_PER_BYTE] & (0x1 << (i % BITS_PER_BYTE))) {
          printf("NFE%u: Sending flow update for flow ID %u \n", nfe, i);
          if (ns_flow_ha_sync_flow(h, &dev, i) != 0)
            fprintf(stderr, "NFE%u: Error sending flow update\n", nfe);
        }
      }

      ns_packet_close_device(dev);
    }
  }

  if (clearmask) {
      for (nfe = 0; nfe < MAX_NFE; nfe++) {
      ret = ns_flow_ha_clear_syncmask(h, nfe);
      if (ret==NS_NFM_SUCCESS)
          printf("Cleared NFE%u syncmask\n", nfe);
      }
  }

  ret = ns_flow_ha_close(&h);
  free(buf);

  return 0;
}
