/*
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * Description: Netronome Flow Module sample application to illustrate
 * SEND_ACTION_VIA_HOST.  Requires nfm_sample_wire (or a similar app)
 * to be running for traffic to pass.  To make all traffic go to the host:
 * ./nfm_sample_rules_via_host
 * ./nfm_sample_wire
 * and inject traffic into port 1, then observe it being transmitted at port 2
 * (or vice versa).
 *
 * @see nfm_sample_rules_test_various_actions.c
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include "nfm_rules.h"

#include "ns_log.h"

#define SUPPORTED_TCAM_KEY_SIZE TCAM_KEY_SIZE_576
#define SUPPORTED_TCAM_KEY_SIZE_STR "576"

#define RQNAME                "/via_host"

// Save the rule names here so we can use them to delete later.
#define NUM_RULES 2
static char rnames[NUM_RULES][RULE_NAME_MAX_LEN];

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -l --loglevel n Set log level\n"
                  " -d --device n   Select NFE device (default 0)\n"
                  " -i --host_id i  Set host destination id (default 15)\n"
                  " -k --keysize size Size of the lookup key (must be " SUPPORTED_TCAM_KEY_SIZE_STR ")\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"host_id",   1, 0, 'i'},
  {"device",    1, 0, 'd'},
  {"keysize",   1, 0, 'k'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

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

 // Add the rule to database
  ret = ns_rule_add_rule(rh, name, prio, pt, kd, act);
  if (ret != NS_NFM_SUCCESS)
    goto fail;

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
  uint32_t num = 0;
  uint32_t prio = 0;
  char *endptr;
  ns_rule_key_size_t key_size = SUPPORTED_TCAM_KEY_SIZE;
  ns_nfm_ret_t ret;
  unsigned int host_id = 15;
  unsigned int device = 0;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:hi:d:k:", __long_options, NULL)) != -1) {
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
    case 'k':
      key_size = strtoul(optarg, &endptr, 0);
      if (*endptr != '\0') {
        fprintf(stderr, "Invalid keysize %s\n", optarg);
        return 1;
      }
      if (key_size != SUPPORTED_TCAM_KEY_SIZE) {
        fprintf(stderr, "Invalid keysize %s, please use " SUPPORTED_TCAM_KEY_SIZE_STR "\n", optarg);
        print_usage(argv[0]);
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

  if (key_size == SUPPORTED_TCAM_KEY_SIZE) {
    /*
     * Reset the configuration.  This is ignored if there was no
     * configuration.  Note that this also flushes the rules from the
     * hardware.
     */
    ret = ns_rules_config_reset(rh);
    if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
      goto fail;
    }

    // Set the desired lookup key size
    ret = ns_rules_config_set_key_size(rh, key_size);
    if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
      goto fail;
    }
  }

  // Send config to rulesd
  ret = ns_rules_setup(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Add a single rule to send traffic to host.
  // Prepare to add the rule
  sprintf(rnames[num], "rule %d", num);
  ret = add_rule(rh, rnames[num], prio, NULL, NULL, SEND_ACTION_VIA_HOST,
                 FST_30_MINUTE_LIST_NUM, host_id);
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

  // Dump
  ret = ns_rule_dump_rules(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Close the rules library.
  ret = ns_rules_close(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  return 0;

fail:
  ns_rules_close(rh);
  return 1;
}
