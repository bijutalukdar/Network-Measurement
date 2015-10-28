/*
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * Description: Netronome Flow Module sample application to illustrate
 * creating TCAM key with IPV6 fields and SEND_ACTION_VIA_HOST.
 *  Requires nfm_sample_wire (or a similar app) to be running for traffic
 * to pass.  To make all traffic go to the host:
 * ./nfm_sample_rules_via_host_v6
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
#include "nfm_error.h"
#include "ns_log.h"

#define RQNAME                "/via_host"

#define CHECK(x)                                                                       \
  do {                                                                                 \
    ns_nfm_ret_t __ret = (x);                                                          \
    if (__ret != 0) {                                                                  \
      NS_LOG_ERROR("Error: %s @%s:%d",ns_nfm_error_string(__ret), __FILE__, __LINE__); \
    }                                                                                  \
  } while(0)



// Save the rule names here so we can use them to delete later.
#define NUM_RULES 2

#define ETHERTYPE_IPV6 0x86dd
#define IPV6_SRC_ADDR  "fe80::213:72ff:fe67:7acf"
#define IPV6_DST_ADDR  "fe80::213:72ff:fe67:7acf"
#define IPV6_SRC_MASK  "::FFFF:FFFF:FFFF:FFFF" // Lower 64 bits used for IPV6 src address
#define IPV6_DST_MASK  "::FF:FFFF:FFFF:FFFF" // Lower 56 bits used for IPV6 Dst address
#define IP_PROTO_UDP   "17"
#define IP_PROTO_TCP   "6"
#define IP_PROTO_ICMPV6   "58"
#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_CODE    0

#define SUPPORTED_TCAM_KEY_SIZE TCAM_KEY_SIZE_576
#define SUPPORTED_TCAM_KEY_SIZE_STR "576"

static char rnames[RULE_NAME_MAX_LEN] = "v6sample";

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
      "\n"
      "Options:\n"
      " -l --loglevel n Set log level\n"
      " -d --device n   Select NFE device (default 0)\n"
      " -i --host_id i  Set host destination id (default 15)\n"
      " -k --keysize size Size of the lookup key (must be " SUPPORTED_TCAM_KEY_SIZE_STR ")\n"
      " -n --name <rname> Rule name to read or write. If omitted defaults to \"v6sample\"\n"
      " -r  Read the rule. Without -n option, reads rule with name \"v6sample\"\n",
      argv0);
  exit(1);
}

//print value or UNSET depending on care
static void print_care(char* name, unsigned int value, int care, const char *fmt)
{
  if(care!=0)
    printf(fmt, name, value);
  else
    printf("%32s = UNSET\n", name);
}

static void print_care_str(char* name, char* value, int care, const char *fmt)
{
  if(care!=0)
    printf(fmt, name, value);
  else
    printf("%32s = UNSET\n", name);
}

static const struct option __long_options[] = {
  {"loglevel",  1, 0, 'l'},
  {"host_id",   1, 0, 'i'},
  {"device",    1, 0, 'd'},
  {"keysize",   1, 0, 'k'},
  {"name",      1, 0, 'n'},
  {"read",      0, 0, 'r'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

static ns_nfm_ret_t add_rule(ns_rule_handle_h *rh,
    char *name,
    uint32_t prio,
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

  // IPV6 Classification API's - Building TCAM key.
  // Set Ether Type - MUST be set for IPV6
  ns_rule_set_etype(kd, ETHERTYPE_IPV6);

  // Set IPV6 Src Address, Mask is optional
  // ns_rule_set_ipv6_sa(kd, IPV6_SRC_ADDR, "");
  ns_rule_set_ipv6_sa(kd, IPV6_SRC_ADDR, IPV6_SRC_MASK);

  // Set IPV6 Dst Address, Mask is optional
  // ns_rule_set_ipv6_da(kd, IPV6_DST_ADDR, "");
  ns_rule_set_ipv6_da(kd, IPV6_DST_ADDR, IPV6_DST_MASK);

  // Set IP protocol type, though API has "ipv4" in it, same API should be used for v4 and v6
  ns_rule_set_ipv4_proto(kd, IP_PROTO_ICMPV6);

  // Set ICMP code
  ns_rule_set_icmp_code(kd, ICMP_ECHO_CODE);

  // Set ICMP type field
  ns_rule_set_icmp_type(kd, ICMP_ECHO_REQUEST); //or ICMP_ECHO_REPLY

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

  printf("\n--- Rule \"%s\" added successfully ---\n\n",name);

  // Free the key data and action data
  (void) ns_rule_free_key_data(kd);
  (void) ns_rule_free_action(act);

  return NS_NFM_SUCCESS;

fail:
  if (kd) ns_rule_free_key_data(kd);
  if (act) ns_rule_free_action(act);
  return NS_NFM_FAIL;
}

static int read_rule(ns_rule_handle_h *rh, char *name)
{
  ns_rule_key_data_h *r_kd = NULL;
  ns_rule_action_h *r_act = NULL;
  ns_rule_cursor_h *ch = NULL;
  ns_nfm_ret_t ret = NS_NFM_SUCCESS;

  char r_name[RULE_NAME_MAX_LEN], proto_str[32], send_act_str[32];
  uint32_t r_prio = 0, r_pt = 0, max_rules, r_committed = 0;
  unsigned int device = 0, host_id, send_action;
  int proto_care, etype_care, icmp_type_care, icmp_code_care;
  uint16_t ethertype;
  uint8_t icmp_type, icmp_code;
  ns_rule_key_size_t key_size;
  ns_rule_buf_mode_t buf_mode;
  flow_direction_t dir;
  int ipv6_len = strlen(DEFAULT_IPV6_MASK)+1;
  char ipv6_sa[ipv6_len];
  char ipv6_sa_mask[ipv6_len];
  char ipv6_da[ipv6_len];
  char ipv6_da_mask[ipv6_len];
  int found = 0,i = 0;


  // Open the rules cursor to read in rules
  ch = ns_rules_open_cursor(rh, NULL, CURSOR_ORDER_PRIO|CURSOR_SOURCE_RULESD);

  r_kd = ns_rule_allocate_key_data(rh);
  if (!r_kd) {
    perror("allocate key failed\n");
    goto fail;
  }
  r_act = ns_rule_allocate_action(rh);
  if (!r_act) {
    perror("allocate action failed\n");
    goto fail;
  }

  // print out general info about rules on this device
  CHECK(ns_rules_config_get_key_size(rh, &key_size));
  CHECK(ns_rules_config_get_buf_mode(rh, &buf_mode));
  CHECK(ns_rules_config_get_max_rules(rh, &max_rules));

  printf("Reading rules ...\n");
  printf("cardid:                    %d\n", device);
  printf("max rules:                 %d\n", max_rules);
  printf("key size:                  %d\n", key_size);
  printf("buffering mode:            %s\n\n", buf_mode==BUF_MODE_SINGLE?"BUF_MODE_SINGLE":"BUF_MODE_DOUBLE");

  strcpy(r_name, "UNSET");

  while(NS_NFM_ERROR_CODE((ret = ns_rule_read(ch, r_name, &r_prio, &r_pt, r_kd, r_act, &r_committed))) != NS_NFM_RULE_EOF) {

    if(strcmp(name,r_name) != 0) {
      continue;
    }

    found = 1;
    printf("%32s = %s\n", "name", r_name);
    CHECK(ns_rule_get_etype(r_kd, &ethertype, &etype_care));
    CHECK(ns_rule_get_ipv6_sa(r_kd, ipv6_sa, ipv6_len, ipv6_sa_mask, ipv6_len));
    CHECK(ns_rule_get_ipv6_da(r_kd, ipv6_da, ipv6_len, ipv6_da_mask, ipv6_len));
    CHECK(ns_rule_get_ipv4_proto_name(r_kd, (char*)&proto_str, 32, &proto_care));

    if (ethertype == ETHERTYPE_IPV6) {
      printf("%32s = %s\n", "ipv6_sa", ipv6_sa);
      printf("%32s = %s\n", "ipv6_sa_mask", ipv6_sa_mask);
      printf("%32s = %s\n", "ipv6_da", ipv6_da);
      printf("%32s = %s\n", "ipv6_da_mask", ipv6_da_mask);
    }

    CHECK(ns_rule_get_icmp_type(r_kd, &icmp_type, &icmp_type_care));
    CHECK(ns_rule_get_icmp_code(r_kd, &icmp_code, &icmp_code_care));
    print_care_str("ipv4_proto", proto_str, proto_care, "%32s = %s\n");
    print_care("icmp_type", icmp_type, icmp_type_care, "%32s = %u\n");
    print_care("icmp_code", icmp_code, icmp_code_care, "%32s = %u\n");
    print_care("etype", ethertype, etype_care, "%32s = 0x%x\n");

    for (i=0; i<2; i++) {
      if(i==0) {
        printf("\n%32s\n", "FLOW_FROM_ORIGINATOR");
        dir=FLOW_FROM_ORIGINATOR;
      }
      else {
        dir=FLOW_FROM_TERMINATOR;
        printf("\n%32s\n", "FLOW_FROM_TERMINATOR");
      }

      CHECK(ns_rule_get_send_action(r_act, &send_action, dir));
      CHECK(ns_rule_get_host_dest_id(r_act, &host_id, dir));

      // Decode values for send action
      switch(send_action){
        case SEND_ACTION_DROP: snprintf(send_act_str, 32, "SEND_ACTION_DROP"); break;
        case SEND_ACTION_DROP_NOTIFY: snprintf(send_act_str, 32, "SEND_ACTION_DROP_NOTIFY"); break;
        case SEND_ACTION_PASS: snprintf(send_act_str, 32, "SEND_ACTION_PASS"); break;
        case SEND_ACTION_VIA_HOST: snprintf(send_act_str, 32, "SEND_ACTION_VIA_HOST"); break;
        case SEND_ACTION_COPY: snprintf(send_act_str, 32, "SEND_ACTION_COPY"); break;
        case SEND_ACTION_COPY_VIA_HOST: snprintf(send_act_str, 32, "SEND_ACTION_COPY_VIA_HOST"); break;
        case SEND_ACTION_HOST_TAP: snprintf(send_act_str, 32, "SEND_ACTION_HOST_TAP"); break;
      }

      printf("%32s = %s\n", "send_action", send_act_str);
      printf("%32s = %d\n", "host_dest_id", host_id);
    }


    break;
  }

  if(!found) {
    printf(" ----- Rule \"%s\" Not Found -----\n", name);
    printf("\n");
  }

  // Free the key data and action data
  (void) ns_rule_free_key_data(r_kd);
  (void) ns_rule_free_action(r_act);

  CHECK(ns_rules_close_cursor(ch));
  return 0;

fail:
  CHECK(ns_rules_close_cursor(ch));
  if(r_kd)  ns_rule_free_key_data(r_kd);
  if(r_act) ns_rule_free_action(r_act);

  return 1;
}

int main(int argc, char *argv[])
{
  ns_rule_handle_h *rh = NULL;
  uint32_t prio = 0,rule_read = 0;
  char *endptr;
  ns_rule_key_size_t key_size = SUPPORTED_TCAM_KEY_SIZE;
  ns_nfm_ret_t ret;
  unsigned int host_id = 15;
  unsigned int device = 0;

  ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
  ns_log_lvl_set(NS_LOG_LVL_INFO);

  int c;
  while ((c = getopt_long(argc, argv, "l:hi:d:k:rn:", __long_options, NULL)) != -1) {
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
      case 'r':
        // Read Rule
        rule_read = 1;
        break;
      case 'n':
        // Rule name to be read or written
        strcpy(rnames, optarg);
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

  if(!rule_read) {
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
    //sprintf(rnames[num], "v6sample %d", num);
    ret = add_rule(rh, rnames, prio, SEND_ACTION_VIA_HOST,
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
  }
  else {

    if(read_rule(rh, rnames)) {
      fprintf(stderr, "failure to read rules @ %s: %d\n", __FILE__, __LINE__);
    }
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
