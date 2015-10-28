/*
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * Description: Netronome Flow Module: A sample app to illustrate
 *              manipulation of TCP and UDP port-related rules.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "nfm_rules.h"
#include "nfm_error.h"

/*
 * Ports are unusual in that you can generate multiple TCAM entries
 * just by using a range. The read API reports the separate TCAM
 * entries as if they are separate rules.
 */

#define RQNAME                "/sample_rules_ports"

typedef struct expected_s {
  uint16_t sport;
  uint16_t smask;
  uint16_t dport;
  uint16_t dmask;
  char name[RULE_NAME_MAX_LEN];
} expected_t;

char *ports = "30-34";
int tcam_entries = 6;
expected_t expected[] = {
  { sport: 30, smask: 0xFFFE, dport: 0,  dmask: 0,      name: "sport rule", },
  { sport: 32, smask: 0xFFFE, dport: 0,  dmask: 0,      name: "sport rule", },
  { sport: 34, smask: 0xFFFF, dport: 0,  dmask: 0,      name: "sport rule", },
  { sport: 0,  smask: 0,      dport: 30, dmask: 0xFFFE, name: "dport rule", },
  { sport: 0,  smask: 0,      dport: 32, dmask: 0xFFFE, name: "dport rule", },
  { sport: 0,  smask: 0,      dport: 34, dmask: 0xFFFF, name: "dport rule", },
};

int main()
{
  ns_rule_handle_h *rh = NULL;
  ns_rule_cursor_h *ch = NULL;
  ns_nfm_ret_t ret = NS_NFM_SUCCESS;
  uint32_t prio = 0;
  uint32_t cardid = 0;
  int i;
  ns_rule_key_data_h *kd = NULL;
  ns_rule_action_h   *act = NULL;

  int exitcode = 0;

  ns_rule_key_data_h *r_kd = NULL;
  ns_rule_action_h   *r_act = NULL;
  char     r_name[RULE_NAME_MAX_LEN];
  uint16_t r_sport;
  uint16_t r_smask;
  uint16_t r_dport;
  uint16_t r_dmask;

  // Init the rules lib
  rh = ns_rules_init(RULESD_Q_NAME, RQNAME, cardid);
  if (!rh) {
    fprintf(stderr, "ns_rules_init() failed\n");
    exitcode = 1;
    return exitcode;
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

  // Add rules
  // We use the same action both times.
  act = ns_rule_allocate_action(rh);
  if (!act)
    goto fail;
  // Set the send action -- what to do with the packets matching the keys
  ret = ns_rule_set_send_action(act, SEND_ACTION_DROP, FLOW_DIRECTION_BOTH);
  if (ret != NS_NFM_SUCCESS)
    goto fail;

  // sport
  kd = ns_rule_allocate_key_data(rh);
  if (!kd) {
    perror("allocate key failed\n");
    return NS_NFM_FAIL;
  }
  ret = ns_rule_set_sport(kd, ports, NULL, NULL);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  // Add the rule to database
  ret = ns_rule_add_rule(rh, "sport rule", prio, RULE_DISCARD, kd, act);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  ns_rule_free_key_data(kd);

  prio++;
  // dport
  kd = ns_rule_allocate_key_data(rh);
  if (!kd) {
    perror("allocate key failed\n");
    return NS_NFM_FAIL;
  }
  ret = ns_rule_set_dport(kd, ports, NULL, NULL);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  // Add the rule to database
  ret = ns_rule_add_rule(rh, "dport rule", prio, RULE_DISCARD, kd, act);
  if (ret != NS_NFM_SUCCESS)
    goto fail;
  ns_rule_free_key_data(kd);

  ns_rule_free_action(act);

  // Commit the rules to hardware.
  ret = ns_rule_commit_rulesdb(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Confirm that the rules we wrote are the rules in the rulesd.
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

  i = 0;
  strcpy(r_name, "UNSET");
  while (NS_NFM_ERROR_CODE((ret = ns_rule_read(ch, r_name, NULL, NULL, r_kd, NULL, NULL))) != NS_NFM_RULE_EOF){
    if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
      goto fail;
    }

    if (0 != strcmp(r_name, expected[i].name)) {
      fprintf(stderr, "failure @ %s: %d; r_name: \"%s\" != expected: \"%s\"\n",
              __FILE__, __LINE__,
              r_name, expected[i].name);
      goto fail;
    }

    ret = ns_rule_get_sport(r_kd, &r_sport, &r_smask);
    if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
      goto fail;
    }

    ret = ns_rule_get_dport(r_kd, &r_dport, &r_dmask);
    if (ret != NS_NFM_SUCCESS) {
      fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
      goto fail;
    }
    printf("name: %s, sport: %d/%#x, dport: %d/%#x\n",
           r_name, r_sport, r_smask, r_dport, r_dmask);

    if (r_sport != expected[i].sport) {
      fprintf(stderr, "failure @ %s: %d: r_sport: %d != expected: %d\n",
              __FILE__, __LINE__, r_sport, expected[i].sport);
      goto fail;
    }
    if (r_smask != expected[i].smask) {
      fprintf(stderr, "failure @ %s: %d: r_smask: %#x != expected: %#x\n",
              __FILE__, __LINE__, r_smask, expected[i].smask);
      goto fail;
    }
    if (r_dport != expected[i].dport) {
      fprintf(stderr, "failure @ %s: %d: r_dport: %d != expected: %d\n",
              __FILE__, __LINE__, r_dport, expected[i].dport);
      goto fail;
    }
    if (r_dmask != expected[i].dmask) {
      fprintf(stderr, "failure @ %s: %d: r_dmask: %#x != expected: %#x\n",
              __FILE__, __LINE__, r_dmask, expected[i].dmask);
      goto fail;
    }

    i++;
  }
  ret = ns_rules_close_cursor(ch);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // Close the rules library.
  ret= ns_rules_close(rh);
  if (ret != NS_NFM_SUCCESS) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  return exitcode;

fail:
  ns_rules_close(rh);
  exitcode = 1;
  return exitcode;
}
