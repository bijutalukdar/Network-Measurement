/**
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 * File:        nfm_sample_platform.c
 * Description: Sample application demonstrating the use of the platform API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "nfm_platform.h"
#include "ns_log.h"


static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};


#define CKRET(ret, msg)                         \
  if (NS_NFM_ERROR_CODE(ret)!=NS_NFM_SUCCESS) { \
    NS_LOG_ERROR(msg " (%d,%d): %s",             \
            NS_NFM_ERROR_CODE(ret),             \
            NS_NFM_ERROR_SUBCODE(ret),          \
            ns_nfm_error_string(ret));          \
    return -1;                                  \
  }

int main(int argc, char* argv[])
{
  ns_nfm_ret_t ret;
  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);
  unsigned int i, k, t, min, max;
  unsigned int slot, netmod, iface, nm_intfs;
  uint32_t nm_mask;
  uint8_t nm_type;
  const char *nm_name;

  int c;
  while ((c = getopt_long(argc, argv, "l:d:h", __long_options, NULL)) != -1) {
    switch (c) {
    case 'l':
      ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc)
    print_usage(argv[0]);

  ret = nfm_get_nfes_present(&k);
  CKRET(ret, "Error getting # of NFEs present");
  printf("%u NFEs present\n", k);

  ret = nfm_get_nmsbs_present(&k);
  CKRET(ret, "Error getting # of NMSBs present");
  printf("%u NMSBs present\n", k);
  if ( k < 1 ) { // No NMSB: geryon by now
      min = max = 1;
      ret = nfm_get_external_min_port(&min);
      CKRET(ret, "Error getting external minimum port");
      
      ret = nfm_get_external_max_port(&max);
      CKRET(ret, "Error getting external maximum port");
      
      printf("Port list\n");
      
      printf("%16s%16s\n", "Port", "Hardware ID");
      for (i = min; i <= max; i++) {
	  ret = nfm_get_internal_port_from_external(i, &k);
	  CKRET(ret, "Error converting external port # to internal");
	  
	  printf("%16u%16u\n", i, k);
	  /* sanity check */
	  ret = nfm_get_external_port_from_internal(k, &t);
	  CKRET(ret, "Error converting internal port # to external");
	  if (t != i)
	      NS_LOG_ERROR("Error conversion ext -> int -> ext "
			   "produced %u -> %u -> %u", i, k, t);
      }
  }
  else { // NMSB: cayenne
      ret = nfm_get_netmods_present(&nm_mask);
      CKRET(ret, "Error getting # of netmods present");
      ret = nfm_get_min_netmod(&min);
      CKRET(ret, "Error getting minimum netmod");
      ret = nfm_get_max_netmod(&max);
      CKRET(ret, "Error getting maximum netmod");

      /* Enumerate the Installed Netmods */
      printf("netmods present (mask=%#x, %u - %u):\n", nm_mask, min, max);
      printf("%16s%16s%16s%16s\n",
	     "Netmod", "Slot ID", "Netmod Type", "# Interfaces");
      while (1) {
	  slot = ffs(nm_mask);
	  if (!slot)
	      break;
	  slot = slot - 1; /* ffs() count bits starting at 1. We don't */
	  nm_mask &= ~(1 << slot);
	  ret = nfm_get_nmsb_attached_card(slot, &nm_type);
	  CKRET(ret, "Error getting card type");
	  ret = nfm_get_nmsb_card_intfs(slot, &nm_intfs);
	  CKRET(ret, "Error getting number of interfaces");
	  ret = nfm_get_nmsb_card_name(slot, &nm_name);
	  CKRET(ret, "Error getting card name");
	  printf("%16s%16u%16u%16u\n", nm_name, slot, nm_type, nm_intfs);
      }
      printf("\n");

      /* Enumerate the ports */
      ret = nfm_get_external_min_port(&min);
      CKRET(ret, "Error getting external minimum port");
      ret = nfm_get_external_max_port(&max);
      CKRET(ret, "Error getting external maximum port");

      printf("Port list\n");
      printf("%16s%16s%16s%16s\n", "Port", "Hardware ID", "Netmod", "NM Interface");
      for (i = min; i <= max; i++) {
	  ret = nfm_get_internal_port_from_external(i, &k);
	  CKRET(ret, "Error converting external port # to internal");
	  ret = nfm_get_slot_from_external(i, &netmod, &iface);
	  CKRET(ret, "Error converting external port # to netmod pair");
	  ret = nfm_get_nmsb_card_name(netmod, &nm_name);
	  CKRET(ret, "Error getting card name");
	  printf("%16u%16u%16s%16u\n", i, k, nm_name, iface);
	  /* sanity check */
	  ret = nfm_get_external_port_from_internal(k, &t);
	  CKRET(ret, "Error converting internal port # to external");
	  if (t != i)
	      NS_LOG_ERROR("Error conversion ext -> int -> ext "
			   "produced %u -> %u -> %u", i, k, t);
      }
  }
  printf("\n");

  return 0;
}
