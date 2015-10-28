/*
 * Copyright (C) 2008-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_get_ports.c
 * Description: Sample application using the rules and loadbalance APIs to
 *              tell which physical ports are associated with a particular
 *              host destination.  For example, if you create an LGID
 *              containing host dest IDs 1,2,3,4,5,6 and create rules for
 *              port 1 and 2 containing this LGID, then this sample
 *              application will return 1 and 2 for inputs of host dest IDs
 *              1-6.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "nfm_rules.h"
#include "nfm_error.h"
#include "ns_log.h"
#include "nfm_loadbalance.h"

#define RQNAME                "/sample_external_ports"

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\nPrint out physical ports associated with a given host destination.\n"
                  "For example if you create an LGID containing host dest IDs 1,2,3,4,5,6\n"
                  "and create rules for port 1 and 2 containing this LGID, then this\n"
                  "sample application will return 1 and 2 for inputs of host dest IDs 1-6.\n"
                  "This assumes that there are 2 NFE devices installed.\n"

                  "\nOptions:\n"
                  " -l --loglevel n    Set log level (default INFO)\n"
                  " -i --host_dest n   Which host destination? (0-31)\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"host_dest", 1, 0, 'i'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

int main(int argc, char* argv[])
{
  ns_rule_handle_h *rh = NULL;
  ns_rule_cursor_h *ch = NULL;
  ns_nfm_ret_t ret;
  unsigned int interface_id, host_dest=0xff;
  int curr_dev = 0, i, exitcode = 0, pp_care;
  int8_t intf_list[256];
  uint32_t lgid=0xff, r_lgid;

  ns_rule_key_data_h *r_kd = NULL;
  ns_rule_action_h *r_act = NULL;
  uint32_t dests[NUM_LOAD_BALANCE_GROUPS];

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:hi:", __long_options, NULL)) != -1) {
    switch (c) {
    case 'l':
      ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
      break;
    case 'i':
      host_dest=atoi(optarg);
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if(host_dest>31) {
    print_usage(argv[0]);
  }

  if (optind != argc)
    print_usage(argv[0]);

  // zero interface list
  memset(intf_list, 0, sizeof(intf_list));

  // This assumes that there are two NFE cards installed
  for(curr_dev=0; curr_dev<2; curr_dev++) {
    // Get all the dests for all LGIDs
    ret=nfm_lb_get_dests(curr_dev, dests);
    if ((ret=nfm_lb_get_dests(curr_dev, dests))!=NS_NFM_SUCCESS) {
      fprintf(stderr, "Error retrieving load balance group destinations from NFE: %s\n", ns_nfm_error_string(ret));
      exit(1);
    }

    // Find the LGID in which the host dest ID is assigned
    for (i=0; i<NUM_LOAD_BALANCE_GROUPS; i++) {
      if (dests[i]) {
        if((1<<host_dest)&dests[i]) {
          lgid=i;
          break;
        }
      }
    }
    // Now search through the rules on both cards looking for the LGID
    // Init the rules lib
    rh = ns_rules_init(RULESD_Q_NAME, RQNAME, curr_dev);
    if (!rh) {
      fprintf(stderr, "ns_rules_init() failed\n");
      exit(1);
    }

    // Open the rules cursor to read in rules
    ch = ns_rules_open_cursor(rh, NULL, CURSOR_ORDER_PRIO|CURSOR_SOURCE_RULESD);

    r_kd = ns_rule_allocate_key_data(rh);
    if (!r_kd) {
      fprintf(stderr, "Allocate key failed: %s\n", ns_nfm_error_string(ret));
      ns_rules_close(rh);
      exit(1);
    }
    r_act = ns_rule_allocate_action(rh);
    if (!r_act) {
      fprintf(stderr, "Allocate key failed: %s\n", ns_nfm_error_string(ret));
      ns_rules_close(rh);
      exit(1);
    }

    while (NS_NFM_ERROR_CODE((ns_rule_read(ch, NULL, NULL, NULL, r_kd, r_act, NULL))) != NS_NFM_RULE_EOF){
      // Get the LGIDs (if any) defined in this rule
      if((ret=ns_rule_get_lgids(r_act, &r_lgid))!=NS_NFM_SUCCESS) {
        fprintf(stderr, "ns_rule_get_lgids() failed: %s\n", ns_nfm_error_string(ret));
        ns_rules_close(rh);
        exit(1);
      }
      // ns_rule_get_lgids returns a bitmask, compare to the LGID found above
      if(!((1<<lgid)&r_lgid)) {
        continue;
      }
      // Now read the physical port associated with the rule
      if((ret=ns_rule_get_addr_space_id(r_kd, &interface_id, &pp_care))!=NS_NFM_SUCCESS) {
        fprintf(stderr, "ns_rule_get_addr_space_id() failed: %s\n", ns_nfm_error_string(ret));
        ns_rules_close(rh);
        exit(1);
      }
      // Save the port to print out later
      if(pp_care) {
        intf_list[interface_id] = 1;
      }
    }
    ns_rules_close_cursor(ch);
    // Close the rules library.
    ns_rules_close(rh);
  }

  printf("Interface IDs feeding host dest %u:\n", host_dest);
  for(interface_id=0; interface_id<sizeof(intf_list)/sizeof(intf_list[0]); interface_id++) {
    if(intf_list[interface_id]) {
      printf("%u ", interface_id);
    }
  }
  printf("\n");

  return exitcode;
}
