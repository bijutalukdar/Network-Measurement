/*
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_rules_keys.c
 * Description: Sample application to demonstrate manipulation of keys
 *              and fields in rules.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <net/ethernet.h>
#include <netinet/tcp.h>

#include "ns_log.h"
#include "nfm_rules.h"
#include "ns_indtbl.h"

#define RQNAME                "/rules_keys"

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
 * Key manipulations sample test.
 *
 * This test program can be used to verify that the lookup keys
 * (fields) are being built correctly.  It uses the library to build
 * up the lookup keys using various fields.  It then reads each
 * field back out of the key structure to confirm that it was written
 * properly. The fields are dumped for manual examination.
 *
 * @see nfm_sample_rules_actions.c
 */
int main(int argc, char *argv[])
{
  ns_rule_key_data_h *kd;
  ns_nfm_ret_t ret;
  ns_rule_handle_h *rh;
  int care;
  unsigned int host_id = 15;
  unsigned int device = 0;
  struct ether_addr read_eth_da;
  uint64_t read_eth_da_mask;
  char tmp1[strlen("00:16:76:BC:C7:F4")+1];
  char tmp2[strlen("00:16:76:BC:C7:F4")+1];

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

  kd = ns_rule_allocate_key_data(rh);
  if (!kd) {
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  // XXX statically allocated buffer which can be overwritten
  struct ether_addr *mac = ether_aton("00:16:76:BC:C7:F4");
  if (!mac) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  ret = ns_rule_set_eth_da(kd, mac, ALL_BITS_VALID);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  ret = ns_rule_get_eth_da(kd, &read_eth_da, &read_eth_da_mask);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_eth_da_mask != ALL_BITS_VALID) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_eth_da_mask: %#llx != ALL_BITS_VALID: %#llx\n", __FILE__, __LINE__, (unsigned long long)read_eth_da_mask, ALL_BITS_VALID);
    goto fail;
  }
  if (0 != memcmp(&read_eth_da, mac, sizeof(read_eth_da))) {
    (void) ns_rule_free_key_data(kd);
    strcpy(tmp1, ether_ntoa(&read_eth_da));
    strcpy(tmp2, ether_ntoa(mac));
    fprintf(stderr, "failure @ %s: %d, read_eth_da: %s != mac: %s\n", __FILE__, __LINE__, tmp1, tmp2);
    goto fail;
  }


  mac = ether_aton("00:11:22:33:44:55");
  if (!mac) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

#define ETH_SA_MASK 0xffffffffULL
  ret = ns_rule_set_eth_sa(kd, mac, ETH_SA_MASK);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  struct ether_addr read_eth_sa;
  uint64_t read_eth_sa_mask;
  ret = ns_rule_get_eth_sa(kd, &read_eth_sa, &read_eth_sa_mask);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_eth_sa_mask != ETH_SA_MASK) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_eth_sa_mask: %#llx != ETH_SA_MASK: %#llx\n", __FILE__, __LINE__, (unsigned long long)read_eth_sa_mask, (unsigned long long)ETH_SA_MASK);
    goto fail;
  }

#define BITSPERBYTE 8
  /* Apply the mask to the mac addr. */
  int i;
  uint16_t tmpmask;
  for (i = 0; i < ETH_ALEN; i++) {
//    tmpmask = (ETH_SA_MASK >> ((i)*BITSPERBYTE)) & ((1<<BITSPERBYTE) - 1);
    tmpmask = (ETH_SA_MASK >> ((ETH_ALEN-1-i)*BITSPERBYTE)) & ((1<<BITSPERBYTE) - 1);
    mac->ether_addr_octet[i] &= tmpmask;
  }

  if (0 != memcmp(&read_eth_sa.ether_addr_octet[0], &mac->ether_addr_octet[0], sizeof(struct ether_addr))) {
    (void) ns_rule_free_key_data(kd);
    strcpy(tmp1, ether_ntoa(&read_eth_sa));
    strcpy(tmp2, ether_ntoa(mac));
    fprintf(stderr, "failure @ %s: %d, read_eth_sa: %s != mac: %s\n", __FILE__, __LINE__, tmp1, tmp2);
    goto fail;
  }

  // ip addresses:
  //     "*" is wildcard
  //     "1.2.3.4" is ok
  //     "1.2.3.4/X" is ok
/* BUG: #define IPV4_DA_STR "192.168.003.159/24" causes set to fail. */
#define IPV4_FULL_LEN    sizeof("192.168.003.159/24")
#define IPV4_DA_STR      "192.168.3.159/24"
#define IPV4_DA_STR_NORM "192.168.3.0/24" // Normalized version of IPV4_DA_STR.
#define IPV4_DA      0x9f03a8c0
#define IPV4_DA_MASK 0x00ffffff
  ret = ns_rule_set_ipv4_da(kd, IPV4_DA_STR);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  uint32_t read_ipv4_da = 0;
  uint32_t read_ipv4_da_mask  = 0;
  ret = ns_rule_get_ipv4_da_num(kd, &read_ipv4_da, &read_ipv4_da_mask);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_ipv4_da_mask != IPV4_DA_MASK) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_da_mask: %#x != IPV4_DA_MASK: %#x\n", __FILE__, __LINE__, read_ipv4_da_mask, IPV4_DA_MASK);
    goto fail;
  }
  if ((read_ipv4_da & read_ipv4_da_mask) != (IPV4_DA & IPV4_DA_MASK)) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_da: %#x != IPV4_DA: %#x\n", __FILE__, __LINE__, read_ipv4_da, IPV4_DA);
    goto fail;
  }

  /* Now do the same test again with the string-based test. */
  char read_ipv4_da_str[IPV4_FULL_LEN];
  ret = ns_rule_get_ipv4_da(kd, read_ipv4_da_str, IPV4_FULL_LEN);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (0 != strcmp(IPV4_DA_STR_NORM, read_ipv4_da_str)) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_da_str: \"%s\" != IPV4_DA_STR_NORM: \"%s\"\n", __FILE__, __LINE__, read_ipv4_da_str, IPV4_DA_STR_NORM);
    goto fail;
  }


  // 192.168.1.1 expressed numerically
#define IPV4_SA_STR      "192.168.1.1"
#define IPV4_SA_STR_NORM "192.168.1.1/32"
#define IPV4_SA       0x0101a8c0
  // The mask is implicit.
#define IPV4_SA_MASK  0xffffffff
  ret = ns_rule_set_ipv4_sa_num(kd, IPV4_SA);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  uint32_t read_ipv4_sa = 0;
  uint32_t read_ipv4_sa_mask  = 0;
  ret = ns_rule_get_ipv4_sa_num(kd, &read_ipv4_sa, &read_ipv4_sa_mask);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_ipv4_sa_mask != IPV4_SA_MASK) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_sa_mask: %#x != IPV4_SA_MASK: %#x\n", __FILE__, __LINE__, read_ipv4_sa_mask, IPV4_SA_MASK);
    goto fail;
  }
  if (read_ipv4_sa != IPV4_SA) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_sa: %#x != IPV4_SA: %#x\n", __FILE__, __LINE__, read_ipv4_sa, IPV4_SA);
    goto fail;
  }

  /* Now do the same test again with the string-based test. */
  char read_ipv4_sa_str[IPV4_FULL_LEN];
  ret = ns_rule_get_ipv4_sa(kd, read_ipv4_sa_str, IPV4_FULL_LEN);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (0 != strcmp(IPV4_SA_STR_NORM, read_ipv4_sa_str)) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_sa_str: \"%s\" != IPV4_SA_STR_NORM: \"%s\"\n", __FILE__, __LINE__, read_ipv4_sa_str, IPV4_SA_STR_NORM);
    goto fail;
  }

  // ip protocol
  //    "name"
  // ret = ns_rule_set_ipv4_proto(kd, "17");
#define IPV4_PROTO "tcp"
  ret = ns_rule_set_ipv4_proto(kd, IPV4_PROTO);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

#define IPV4_PROTO_MAX_LEN 1024 /* I just made this up. */
  char read_ipv4_proto[IPV4_PROTO_MAX_LEN];
  care = 0;
  ret = ns_rule_get_ipv4_proto_name(kd, read_ipv4_proto, IPV4_PROTO_MAX_LEN, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, ipv4_proto should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (0 != strncmp(read_ipv4_proto, IPV4_PROTO, IPV4_PROTO_MAX_LEN)) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_proto: \"%s\" != IPV4_PROTO: \"%s\"\n", __FILE__, __LINE__, read_ipv4_proto, IPV4_PROTO);
    goto fail;
  }


  // ethertype
  ret = ns_rule_set_etype(kd, ETHERTYPE_ARP);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  uint16_t read_etype = 0;
  care = 0;
  ret = ns_rule_get_etype(kd, &read_etype, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, etype should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_etype != ETHERTYPE_ARP) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_etype: %#x != ETHERTYPE_ARP: %#x\n", __FILE__, __LINE__, read_etype, ETHERTYPE_ARP);
    goto fail;
  }

#define ADDR_SPACE_ID 1
  ret = ns_rule_set_addr_space_id(kd, ADDR_SPACE_ID);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int read_addr_space_id = 1;
  care = 0;
  ret = ns_rule_get_addr_space_id(kd, &read_addr_space_id, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, addr_space_id should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_addr_space_id != ADDR_SPACE_ID) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_addr_space_id: %#x != ADDR_SPACE_ID: %#x\n", __FILE__, __LINE__, read_addr_space_id, ADDR_SPACE_ID);
    goto fail;
  }

#define INGRESS_INTF 1
  ret = ns_rule_set_ingress_interface(kd, INGRESS_INTF, 1);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int read_ingress_logical_intf = 0;
  unsigned int read_ingress_logical = 0;
  care = 0;
  ret = ns_rule_get_ingress_interface(kd, &read_ingress_logical_intf, &read_ingress_logical, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, ingress_logical_intf should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_ingress_logical_intf != INGRESS_INTF) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ingress_logical_intf: %#x != INGRESS_INTF: %#x\n", __FILE__, __LINE__, read_ingress_logical_intf, INGRESS_INTF);
    goto fail;
  }
  if (read_ingress_logical!= 1) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ingress_logical: %#x != 1\n", __FILE__, __LINE__, read_ingress_logical_intf);
    goto fail;
  }

#define EGRESS_INTF 1
  ret = ns_rule_set_egress_interface(kd, EGRESS_INTF, 1);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int read_egress_logical_intf = 0;
  unsigned int read_egress_logical = 0;
  care = 0;
  ret = ns_rule_get_egress_interface(kd, &read_egress_logical_intf, &read_egress_logical, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, egress_logical_intf should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_egress_logical_intf != EGRESS_INTF) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_egress_logical_intf: %#x != EGRESS_INTF: %#x\n", __FILE__, __LINE__, read_egress_logical_intf, EGRESS_INTF);
    goto fail;
  }
  if (read_egress_logical!= 1) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_egress_logical: %#x != 1\n", __FILE__, __LINE__, read_egress_logical_intf);
    goto fail;
  }
//--
  ret = ns_rule_set_ingress_interface(kd, INGRESS_INTF, 0);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int read_ingress_physical_intf = 0;
  unsigned int read_ingress_physical = 1;
  care = 0;
  ret = ns_rule_get_ingress_interface(kd, &read_ingress_physical_intf, &read_ingress_physical, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, ingress_physical_intf should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_ingress_physical_intf != INGRESS_INTF) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ingress_physical_intf: %#x != INGRESS_INTF: %#x\n", __FILE__, __LINE__, read_ingress_physical_intf, INGRESS_INTF);
    goto fail;
  }
  if (read_ingress_physical!= 0) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ingress_physical: %#x != 0\n", __FILE__, __LINE__, read_ingress_physical_intf);
    goto fail;
  }

  ret = ns_rule_set_egress_interface(kd, EGRESS_INTF, 0);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int read_egress_physical_intf = 0;
  unsigned int read_egress_physical = 0;
  care = 0;
  ret = ns_rule_get_egress_interface(kd, &read_egress_physical_intf, &read_egress_physical, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, egress_physical_intf should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_egress_physical_intf != EGRESS_INTF) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_egress_physical_intf: %#x != EGRESS_INTF: %#x\n", __FILE__, __LINE__, read_egress_physical_intf, EGRESS_INTF);
    goto fail;
  }
  if (read_egress_physical!= 0) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_egress_physical: %#x != 0\n", __FILE__, __LINE__, read_egress_physical_intf);
    goto fail;
  }

#define VLANID 1000
  ret = ns_rule_set_vlanid(kd, VLANID);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  unsigned int read_vlanid = 1;
  care = 0;
  ret = ns_rule_get_vlanid(kd, &read_vlanid, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, vlanid should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_vlanid != VLANID) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_vlanid: %#x != VLANID: %#x\n", __FILE__, __LINE__, read_vlanid, VLANID);
    goto fail;
  }

  // dscp
  uint32_t dscp = 0x22;
  ret = ns_rule_set_ipv4_dscp(kd, dscp);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  uint32_t read_ipv4_dscp = 0;
  care = 0;
  ret = ns_rule_get_ipv4_dscp(kd, &read_ipv4_dscp, &care);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (!care) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, ipv4_dscp should care but doesn't\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_ipv4_dscp != dscp) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_ipv4_dscp: %#x != dscp: %#x\n", __FILE__, __LINE__, read_ipv4_dscp, dscp);
    goto fail;
  }

#define TCP_FLAGS  (~TH_URG & (TH_PUSH | TH_SYN))
#define TCP_MASK   ( TH_URG |  TH_PUSH | TH_SYN )
  ret = ns_rule_set_tcp_flags(kd, TCP_FLAGS, TCP_MASK);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  uint8_t read_tcp_flags = 0;
  uint8_t read_tcp_mask  = 0;
  ret = ns_rule_get_tcp_flags(kd, &read_tcp_flags, &read_tcp_mask);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }
  if (read_tcp_mask != TCP_MASK) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_tcp_mask: %#x != TCP_MASK: %#x\n", __FILE__, __LINE__, read_tcp_mask, TCP_MASK);
    goto fail;
  }
  if (read_tcp_flags != TCP_FLAGS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d, read_tcp_flags: %#x != TCP_FLAGS: %#x\n", __FILE__, __LINE__, read_tcp_flags, TCP_FLAGS);
    goto fail;
  }

  // ports have to be last (See bug report 577)
  //     "*" is wildcard
  //     "X" (single value converted to int)
  //     "A, B, C:D, X:Y" (comma separated range tokens)
  //     min/max expressed by either a ":" or a "-".
  ret = ns_rule_set_sport(kd, "30-34", NULL, NULL);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  ret = ns_rule_set_dport(kd, "2000-2002", NULL, NULL);
  if (ret != NS_NFM_SUCCESS) {
    (void) ns_rule_free_key_data(kd);
    fprintf(stderr, "failure @ %s: %d\n", __FILE__, __LINE__);
    goto fail;
  }

  ns_rule_dump_key_data(kd);

  (void) ns_rule_free_key_data(kd);

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
