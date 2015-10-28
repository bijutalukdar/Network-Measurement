/*
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * Description: Netronome Flow Module sample application to illustrate
 *              SEND_ACTION_PASS.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "ns_log.h"
#include "nfm_rules.h"
#include "ns_indtbl.h"

#define RQNAME                "/rules_pass"

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

// Save the rule names here so we can use them to delete later.
#define NUM_RULES 2
static char rnames[NUM_RULES][RULE_NAME_MAX_LEN];

static ns_nfm_ret_t add_rule(ns_rule_handle_h *rh, char *name,
                             uint32_t prio, char *ip __attribute__((unused)),
                             char *dport __attribute__((unused)),
                             send_action_t sa,
                             flow_timeout_t ft,
                             unsigned int host_id)
{
  ns_rule_key_data_h *kd = NULL;
  ns_rule_action_h *act = NULL;
  ns_nfm_ret_t ret = NS_NFM_SUCCESS;
  ns_rule_persistent_t pt = RULE_DISCARD;

  // Allocate key data to build and store our lookup keys
  kd = ns_rule_allocate_key_data(rh);
  if (!kd) {
    perror("allocate key failed\n");
    return NS_NFM_FAIL;
  }
  // Allocate an action structure
  act = ns_rule_allocate_action(rh);
  if (!act)
    goto fail;
  // Set the flow timeout value
  ret = ns_rule_set_flow_timeout(act, ft);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  // Set the send action -- what to do with the packets matching the keys
  ret = ns_rule_set_send_action(act, sa, FLOW_DIRECTION_BOTH);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  // Set the destination ID for Host
  ret = ns_rule_set_host_dest_id(act, host_id, FLOW_DIRECTION_BOTH);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  // Enable SOF and EOF messages.
  ret = ns_rule_enable_start_of_flow_message(act);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  ret = ns_rule_enable_end_of_flow_message(act);
  if (ret != NS_NFM_SUCCESS)
    goto fail;

  // Add the rule to database
  ret = ns_rule_add_rule(rh, name, prio, pt, kd, act);

  // Free the key data and action data
  (void) ns_rule_free_key_data(kd);
  (void) ns_rule_free_action(act);

  return NS_NFM_SUCCESS;

fail:
  if (kd) ns_rule_free_key_data(kd);
  if (act) ns_rule_free_action(act);
  return NS_NFM_FAIL;
}

int main(int argc, char *argv[])
{
  ns_rule_handle_h *rh = NULL;
  ns_nfm_ret_t ret = NS_NFM_SUCCESS;
  uint32_t num = 0;
  uint32_t prio = 0;
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

  // Init the rules lib
  rh = ns_rules_init(RULESD_Q_NAME, RQNAME, device);
  if (!rh) {
    fprintf(stderr, "ns_rules_init() failed\n");
    return 1;
  }

  // Send default config to rulesd
  ret = ns_rules_setup(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Flush all the existing rules.
  ret = ns_rule_flush_hw(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Add a single rule to send traffic to host.
  // Prepare to add the rule
  sprintf(rnames[num], "rule %d", num);
  ret = add_rule(rh, rnames[num], prio, NULL, NULL, SEND_ACTION_PASS,
                 FST_30_SECOND_LIST_NUM, host_id);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Commit
  ret = ns_rule_commit_rulesdb(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Close the rules library.
  ns_rules_close(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  return 0;

fail:
  ns_rules_close(rh);
  return 1;
}
