/*
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * Description: Netronome Flow Module: Manipulate actions
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

#define RQNAME                "/rules_actions"

/* Round up x to n (n is a power of 2) */
#define ROUNDUP(x, n)       ((x + n - 1) & ~(n-1))


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

/*
 * Actions manipulations sample test.
 *
 * This test program can be used to verify that the action structure
 * is built up correctly.  It uses the rules library to build up the
 * action data structure and dumps the output for manual examination.
 *
 * @see nfm_sample_rules_test_various_actions.c
 * @see nfm_sample_rules_keys.c
 */
int main(int argc, char *argv[])
{
  int final_result = 0;
  ns_rule_action_h *act;
  ns_nfm_ret_t ret;
  ns_rule_handle_h *rh;
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

  // Send config to rulesd (accepts default configuration)
  ret = ns_rules_setup(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  act = ns_rule_allocate_action(rh);
  if (!act) {
    perror("allocate action failed\n");
    return 1;
  }

  // Flows will timeout after two hours
  ret = ns_rule_set_flow_timeout(act, FST_2_HOUR_LIST_NUM);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  flow_timeout_t ft;
  ret = ns_rule_get_flow_timeout(act, &ft);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (FST_2_HOUR_LIST_NUM != ft) {
    fprintf(stderr, "failure @ %s: %d FST_2_HOUR_LIST_NUM: %d != ft: %d\n", __FILE__, __LINE__, FST_2_HOUR_LIST_NUM, ft);
    final_result = 1;
  }

  // Enable start-of-flow message from NFE to Host
  ret = ns_rule_enable_start_of_flow_message(act);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int start_of_flow_message_flag;
  ret = ns_rule_get_start_of_flow_message(act, &start_of_flow_message_flag);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!start_of_flow_message_flag) {
    fprintf(stderr, "failure @ %s: %d !start_of_flow_message_flag: %d\n", __FILE__, __LINE__, start_of_flow_message_flag);
    final_result = 1;
  }

  // Enable end-of-flow message from NFE to Host
  ret = ns_rule_enable_end_of_flow_message(act);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int end_of_flow_message_flag;
  ret = ns_rule_get_end_of_flow_message(act, &end_of_flow_message_flag);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!end_of_flow_message_flag) {
    fprintf(stderr, "failure @ %s: %d !end_of_flow_message_flag: %d\n", __FILE__, __LINE__, end_of_flow_message_flag);
    final_result = 1;
  }

  // ns_rule_enable_flow_counter() is missing.
  // ns_rule_enable_bypass_host_if_unavailable() is missing.

  // Enable reclassification.
  ret = ns_rule_enable_reclassify(act);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int reclassify_flag;
  ret = ns_rule_get_reclassify(act, &reclassify_flag);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!reclassify_flag) {
    fprintf(stderr, "failure @ %s: %d !reclassify_flag: %d\n", __FILE__, __LINE__, reclassify_flag);
    final_result = 1;
  }

  // This is ignored by the NFE, used on the host side opaquely.
#define USER_RULE_CONTEXT 0x12345678
  ret = ns_rule_set_user_rule_context(act, USER_RULE_CONTEXT);
  if (ret != NS_NFM_SUCCESS)
    return 1;

  uint32_t user_rule_context;
  ret = ns_rule_get_user_rule_context(act, &user_rule_context);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (USER_RULE_CONTEXT != user_rule_context) {
    fprintf(stderr, "failure @ %s: %d USER_RULE_CONTEXT: %#x != user_rule_context: %#x\n", __FILE__, __LINE__, USER_RULE_CONTEXT, user_rule_context);
    final_result = 1;
  }

  // Test Load-balancing calls.
  {
    uint32_t lgids = LGID00|LGID01;
    /* Enable hardware load-balancing for this rule. */
    ret = ns_rule_set_lgids(act, lgids);
    if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
      goto fail;
    }

    uint32_t r_lgids;
    ret = ns_rule_get_lgids(act, &r_lgids);
    if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
      goto fail;
    }
    if (lgids != r_lgids) {
      fprintf(stderr, "failure @ %s: %d lgids: %#x != r_lgids: %#x\n", __FILE__, __LINE__,
              lgids, r_lgids);
      final_result = 1;
    }
  }

  // Set the send action.
  ret = ns_rule_set_send_action(act, SEND_ACTION_PASS, FLOW_DIRECTION_BOTH);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  uint32_t send_action;
  ret = ns_rule_get_send_action(act, &send_action, FLOW_FROM_ORIGINATOR);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (SEND_ACTION_PASS != send_action) {
    fprintf(stderr, "failure @ %s: %d FLOW_FROM_ORIGINATOR SEND_ACTION_PASS: %d != send_action: %d\n", __FILE__, __LINE__, SEND_ACTION_PASS, send_action);
    final_result = 1;
  }

  ret = ns_rule_get_send_action(act, &send_action, FLOW_FROM_TERMINATOR);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (SEND_ACTION_PASS != send_action) {
    fprintf(stderr, "failure @ %s: %d FLOW_FROM_TERMINATOR SEND_ACTION_PASS: %d != send_action: %d\n", __FILE__, __LINE__, SEND_ACTION_PASS, send_action);
    final_result = 1;
  }


  // tcpdump style snaplen
#define SNAPLEN 65
  ret = ns_rule_set_snaplen(act, SNAPLEN, FLOW_DIRECTION_BOTH);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  uint32_t snaplen;
  ret = ns_rule_get_snaplen(act, &snaplen, FLOW_FROM_ORIGINATOR);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (ROUNDUP(SNAPLEN, SNAPLEN_UNITS) != snaplen) {
    fprintf(stderr, "failure @ %s: %d FLOW_FROM_ORIGINATOR ROUNDUP(SNAPLEN: %d, 8): %d != snaplen: %d\n", __FILE__, __LINE__, SNAPLEN, ROUNDUP(SNAPLEN, SNAPLEN_UNITS), snaplen);
    final_result = 1;
  }

  ret = ns_rule_get_snaplen(act, &snaplen, FLOW_FROM_TERMINATOR);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (ROUNDUP(SNAPLEN, SNAPLEN_UNITS) != snaplen) {
    fprintf(stderr, "failure @ %s: %d FLOW_FROM_TERMINATOR ROUNDUP(SNAPLEN: %d, 8): %d != snaplen: %d\n", __FILE__, __LINE__, SNAPLEN, ROUNDUP(SNAPLEN, 8), snaplen);
    final_result = 1;
  }

  // ns_rule_set_qos_index() is missing.
  // ns_rule_set_queue_id() is missing.

  // Where to send if we are sending this to Host?
  ret = ns_rule_set_host_dest_id(act, host_id, FLOW_DIRECTION_BOTH);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int host_dest_id;
  ret = ns_rule_get_host_dest_id(act, &host_dest_id, FLOW_FROM_ORIGINATOR);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (host_id != host_dest_id) {
    fprintf(stderr, "failure @ %s: %d FLOW_FROM_ORIGINATOR host_id: %d != host_dest_id: %d\n", __FILE__, __LINE__, host_id, host_dest_id);
    final_result = 1;
  }

  ret = ns_rule_get_host_dest_id(act, &host_dest_id, FLOW_FROM_TERMINATOR);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (host_id != host_dest_id) {
    fprintf(stderr, "failure @ %s: %d FLOW_FROM_TERMINATOR host_id: %d != host_dest_id: %d\n", __FILE__, __LINE__, host_id, host_dest_id);
    final_result = 1;
  }

  ret = ns_rule_get_host_dest_id(act, &host_dest_id, FLOW_DIRECTION_BOTH);
  if (ret == NS_NFM_SUCCESS) {
    fprintf(stderr, "Unexpected success @ %s: %d\n", __FILE__, __LINE__);
    final_result = 1;
  }

  // ns_rule_enable_copy_to_host() is missing.

  // Close the rules library.
  ns_rules_close(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  return final_result;

fail:
  ns_rules_close(rh);
  return final_result;
}
