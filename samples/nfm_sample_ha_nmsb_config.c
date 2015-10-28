/*
 * Copyright (C) 2012 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_ha_nmsb_config.c
 * Description: Sample application to configure NMSB TCAM rules and load-balance
 *              groups to direct HA FST packets, incoming on the HA IO port,
 *              to the correct NFE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "ns_log.h"
#include "scmd.h"
#include "ns_ha_nmsb_mappings.h"

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "Configure the NMSB TCAM as well as two NMSB load-balancing"
                  " groups, in order to get the HA FST update packets to land on"
                  " the correct NFE as flows get synchronized across the HA link."
                  " Two new NMSB TCAM rules will select the FST update packets"
                  " incoming on the specified HA port and direct them to the new "
                  " load-balancing groups.                                       "
                  "\n\n"
                  "Options:\n"
                  " -p --ha_port        The IO port on the NMSB used for the HA Link\n"
                  " -t --tcam_index     The NMSB tcam index, t, to use for the HA rules. (default is 18)\n"
                  "                     (indexes t and t+1 will be used and overwritten)\n"
                  " -i --lbg_index      The NMSB load-balance group index, i, to use. (default is 3)\n"
                  "                     (indexes i and i+1 will be used and overwritten)\n"
                  " -s --stack_size     The number of appliances in this stack\n"
                  " -n --nfes           The number of NFEs per appliance\n"
                  " -l --loglevel n  Set log level\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"ha_port",1, 0, 'p'},
  {"loglevel",  1, 0, 'l'},
  {"tcam_index",  1, 0, 't'},
  {"stack_size",  1, 0, 's'},
  {"nfes",  1, 0, 'n'},
  {"lbg_index",  1, 0, 'i'},
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

#define MAX_NFE         4

static int open_scmd_handle(struct sc_desc *sd, uint32_t app_id)
{
    int rc;
    memset(sd, 0, sizeof (struct sc_desc));
    rc = scmd_open(sd, app_id);
    if (rc != 0) {
        fprintf(stderr, "Error with scmd_open %s", scmd_strerror(rc));
        if ( (rc < 0) && (errno == EAGAIN) ) {
            fprintf(stderr, "Have the daemons been started on the NMSB card?");
        }
        return -1;
    }
    return 0;
}

static int print_lb_group(struct sc_lb_group *lbg)
{
    int i;
    if (!lbg)
        return -1;

    fprintf(stdout, "Load balance group %u: {", lbg->index);
    for (i = 0; i < lbg->size; i++) {
        fprintf(stdout, "%u.%u,",lbg->dev[i],lbg->ports[i]);
    }
    fprintf(stdout, "}\n");
    return 0;
}


static int create_load_balance_groups(struct sc_desc *sd,
                                      struct sc_lb_group *lbg1,
                                      struct sc_lb_group *lbg2,
                                      const int lbg_index,
                                      const int stack_size,
                                      const int nfes)
{
    int rc;

    memset(lbg1, 0, sizeof(struct sc_lb_group));
    memset(lbg2, 0, sizeof(struct sc_lb_group));
    lbg1->index = lbg_index;
    lbg2->index = lbg_index+1;

    switch(stack_size) {
        case 1:
            switch (nfes) {
                case 1:
                    lbg1->size = 2;
                    lbg2->size = 2;
                    memcpy(lbg1->dev, dev_1nfe_1box, sizeof(dev_1nfe_1box));
                    memcpy(lbg2->dev, dev_1nfe_1box, sizeof(dev_1nfe_1box));
                    memcpy(lbg1->ports, port_1nfe_1box_1, sizeof(port_1nfe_1box_1));
                    memcpy(lbg2->ports, port_1nfe_1box_2, sizeof(port_1nfe_1box_2));
                    break;
                case 2:
                    lbg1->size = 4;
                    lbg2->size = 4;
                    memcpy(lbg1->dev, dev_2nfe_1box, sizeof(dev_2nfe_1box));
                    memcpy(lbg2->dev, dev_2nfe_1box, sizeof(dev_2nfe_1box));
                    memcpy(lbg1->ports, port_2nfe_1box_1, sizeof(port_2nfe_1box_1));
                    memcpy(lbg2->ports, port_2nfe_1box_2, sizeof(port_2nfe_1box_2));
                    break;
                case 4:
                    lbg1->size = 4;
                    lbg2->size = 4;
                    memcpy(lbg1->dev, dev_4nfe_1box, sizeof(dev_4nfe_1box));
                    memcpy(lbg2->dev, dev_4nfe_1box, sizeof(dev_4nfe_1box));
                    memcpy(lbg1->ports, port_4nfe_1box_1, sizeof(port_4nfe_1box_1));
                    memcpy(lbg2->ports, port_4nfe_1box_2, sizeof(port_4nfe_1box_2));
                    break;
            }
            break;
        case 2:
            switch (nfes) {
                case 1:
                    lbg1->size = 2;
                    lbg2->size = 2;
                    memcpy(lbg1->dev, dev_1nfe_2box, sizeof(dev_1nfe_2box));
                    memcpy(lbg2->dev, dev_1nfe_2box, sizeof(dev_1nfe_2box));
                    memcpy(lbg1->ports, port_1nfe_2box_1, sizeof(port_1nfe_2box_1));
                    memcpy(lbg2->ports, port_1nfe_2box_2, sizeof(port_1nfe_2box_2));
                    break;
                case 2:
                    lbg1->size = 8;
                    lbg2->size = 8;
                    memcpy(lbg1->dev, dev_2nfe_2box, sizeof(dev_2nfe_2box));
                    memcpy(lbg2->dev, dev_2nfe_2box, sizeof(dev_2nfe_2box));
                    memcpy(lbg1->ports, port_2nfe_2box_1, sizeof(port_2nfe_2box_1));
                    memcpy(lbg2->ports, port_2nfe_2box_2, sizeof(port_2nfe_2box_2));
                    break;
                case 4:
                    lbg1->size = 8;
                    lbg2->size = 8;
                    memcpy(lbg1->dev, dev_4nfe_2box, sizeof(dev_4nfe_2box));
                    memcpy(lbg2->dev, dev_4nfe_2box, sizeof(dev_4nfe_2box));
                    memcpy(lbg1->ports, port_4nfe_2box_1, sizeof(port_4nfe_2box_1));
                    memcpy(lbg2->ports, port_4nfe_2box_2, sizeof(port_4nfe_2box_2));
                    break;
            }
            break;
        case 3:
            switch (nfes) {
                case 1:
                    lbg1->size = 6;
                    lbg2->size = 6;
                    memcpy(lbg1->dev, dev_1nfe_3box, sizeof(dev_1nfe_3box));
                    memcpy(lbg2->dev, dev_1nfe_3box, sizeof(dev_1nfe_3box));
                    memcpy(lbg1->ports, port_1nfe_3box_1, sizeof(port_1nfe_3box_1));
                    memcpy(lbg2->ports, port_1nfe_3box_2, sizeof(port_1nfe_3box_2));
                    break;
                case 2:
                    lbg1->size = 6;
                    lbg2->size = 6;
                    memcpy(lbg1->dev, dev_2nfe_3box, sizeof(dev_2nfe_3box));
                    memcpy(lbg2->dev, dev_2nfe_3box, sizeof(dev_2nfe_3box));
                    memcpy(lbg1->ports, port_2nfe_3box_1, sizeof(port_2nfe_3box_1));
                    memcpy(lbg2->ports, port_2nfe_3box_2, sizeof(port_2nfe_3box_2));
                    break;
                case 4:
                    lbg1->size = 6;
                    lbg2->size = 6;
                    memcpy(lbg1->dev, dev_4nfe_3box, sizeof(dev_4nfe_3box));
                    memcpy(lbg2->dev, dev_4nfe_3box, sizeof(dev_4nfe_3box));
                    memcpy(lbg1->ports, port_4nfe_3box_1, sizeof(port_4nfe_3box_1));
                    memcpy(lbg2->ports, port_4nfe_3box_2, sizeof(port_4nfe_3box_2));
                    break;
            }
            break;
        case 4:
            switch (nfes) {
                case 1:
                    return -1;
                case 2:
                    lbg1->size = 8;
                    lbg2->size = 8;
                    memcpy(lbg1->dev, dev_2nfe_4box, sizeof(dev_2nfe_4box));
                    memcpy(lbg2->dev, dev_2nfe_4box, sizeof(dev_2nfe_4box));
                    memcpy(lbg1->ports, port_2nfe_4box_1, sizeof(port_2nfe_4box_1));
                    memcpy(lbg2->ports, port_2nfe_4box_2, sizeof(port_2nfe_4box_2));
                    break;
                case 4:
                    lbg1->size = 8;
                    lbg2->size = 8;
                    memcpy(lbg1->dev, dev_4nfe_4box, sizeof(dev_4nfe_4box));
                    memcpy(lbg2->dev, dev_4nfe_4box, sizeof(dev_4nfe_4box));
                    memcpy(lbg1->ports, port_4nfe_4box_1, sizeof(port_4nfe_4box_1));
                    memcpy(lbg2->ports, port_4nfe_4box_2, sizeof(port_4nfe_4box_2));
                    break;
            }
            break;
        default:
            return 1;
    }

    /* Now programe the new load balance groups to the NMSB */
    rc = scmd_set_lb_group(sd, lbg1);
    if (rc != 0) {
        fprintf(stderr, "Error setting load-balance group 1: %s\n", scmd_strerror(rc));
        return 1;
    }
    rc = scmd_set_lb_group(sd, lbg2);
    if (rc != 0) {
        fprintf(stderr, "Error setting load-balance group 2: %s\n", scmd_strerror(rc));
        return 1;
    }
    /* Check that they were set properly */
    struct sc_lb_group tmp_lbg;
    memset(&tmp_lbg, 0, sizeof(struct sc_lb_group));
    tmp_lbg.index = lbg_index;
    rc = scmd_read_lb_group(sd, &tmp_lbg);
    if (rc != 0 || memcmp(lbg1, &tmp_lbg, sizeof(struct sc_lb_group)) != 0) {
        fprintf(stderr, "Error reading load-balance group 1: %s\n", scmd_strerror(rc));
        return 1;
    }
    print_lb_group(&tmp_lbg);
    memset(&tmp_lbg, 0, sizeof(struct sc_lb_group));
    tmp_lbg.index = lbg_index+1;
    rc = scmd_read_lb_group(sd, &tmp_lbg);
    if (rc != 0 || memcmp(lbg2, &tmp_lbg, sizeof(struct sc_lb_group)) != 0) {
        fprintf(stderr, "Error reading load-balance group 2: %s\n", scmd_strerror(rc));
        return 1;
    }
    print_lb_group(&tmp_lbg);

    return 0;
}

static int create_tcam_rules(struct sc_desc *sd,
                             const int ha_port,
                             const int tcam_index,
                             const int lbg_index)
{

    int rc;
    uint8_t kt_nonip, kt_ipv4, kt_ipv6;
    struct sc_TCAM_key_nonip_24B_L2_key0 lbg1_key;
    struct sc_TCAM_key_nonip_24B_L2_key0 lbg2_key;
    struct sc_TCAM_entry entry1;
    struct sc_TCAM_entry entry2;

    /* If adding a rule, you must first do a read of the key type
       with the same descriptor that you will do the add with. */
    rc = scmd_read_TCAM_key_type(sd, &kt_nonip, &kt_ipv4, &kt_ipv6);
    if (rc != 0) {
        fprintf(stderr, "Error with scmd_read_TCAM_key_type: %s\n", scmd_strerror(rc));
        return -1;
    }

    memset(&lbg1_key, 0, sizeof(lbg1_key));
    memset(&lbg2_key, 0, sizeof(lbg2_key));
    lbg1_key.action.PCEEgressInterface = lbg_index;
    lbg2_key.action.PCEEgressInterface = lbg_index+1;
    lbg1_key.action.PCETargetIsTrunk = 1;
    lbg2_key.action.PCETargetIsTrunk = 1;

    lbg1_key.data.SrcPort = ha_port;
    lbg2_key.data.SrcPort = ha_port;
    lbg1_key.data.isL2Valid = 1;
    lbg2_key.data.isL2Valid = 1;
    lbg1_key.data.L2EncapType = 1;
    lbg2_key.data.L2EncapType = 1;
    lbg1_key.data.EtherType = 0x4319;
    lbg2_key.data.EtherType = 0x4319;
    lbg1_key.data.SA.addr[0] = HA_NMSB_LB_TABLE1_MSBYTE;
    lbg2_key.data.SA.addr[0] = HA_NMSB_LB_TABLE2_MSBYTE;
    lbg1_key.mask.SrcPort = 0x3f;
    lbg2_key.mask.SrcPort = 0x3f;
    lbg1_key.mask.SA.addr[0] = 0xFF;
    lbg2_key.mask.SA.addr[0] = 0xFF;

    memset(&entry1, 0, sizeof(entry1));
    memset(&entry2, 0, sizeof(entry2));
    rc = scmd_pack_TCAM_entry_nonip_24B_L2_key0(sd, &lbg1_key, &entry1);
    if (rc != 0) {
        fprintf(stderr,"Error packing TCAM entry for load-balance group %i: %s\n",lbg_index, scmd_strerror(rc));
        return -1;
    }
    rc = scmd_write_tcam_entry(sd, tcam_index, &entry1);
    if (rc != 0) {
        fprintf(stderr,"Error writing TCAM entry for load-balance group %i: %s\n",lbg_index, scmd_strerror(rc));
        return -1;
    }
    rc = scmd_pack_TCAM_entry_nonip_24B_L2_key0(sd, &lbg2_key, &entry2);
    if (rc != 0) {
        fprintf(stderr,"Error packing TCAM entry for load-balance group %i: %s\n",lbg_index+1, scmd_strerror(rc));
        return -1;
    }
    rc = scmd_write_tcam_entry(sd, tcam_index+1, &entry2);
    if (rc != 0) {
        fprintf(stderr,"Error writing TCAM entry for load-balance group %i: %s\n",lbg_index+1, scmd_strerror(rc));
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct sc_desc sd;
    struct sc_lb_group lbg1;
    struct sc_lb_group lbg2;
    int ha_port = -1;
    int stack_size = -1;
    int nfes = -1;
    int tcam_index = 18;
    int lbg_index = 3;

    ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
    ns_log_lvl_set(NS_LOG_LVL_INFO);

    int c;
    while ((c = getopt_long(argc, argv, "p:l:ht:s:n:i:", __long_options, NULL)) != -1) {
        switch (c) {
            case 'p':
                ha_port = strtol(optarg,0,0);
                break;
            case 'l':
                ns_log_lvl_set((unsigned int)strtoul(optarg,0,0));
                break;
            case 't':
                tcam_index = strtol(optarg,0,0);
                break;
            case 's':
                stack_size = strtol(optarg,0,0);
                break;
            case 'n':
                nfes = strtol(optarg,0,0);
                break;
            case 'i':
                lbg_index = strtol(optarg,0,0);
                break;
            case 'h':
            default:
                print_usage(argv[0]);
        }
    }

    if (optind != argc || argc == 1)
    print_usage(argv[0]);

    if (ha_port < 0 || ha_port > SCMD_MAX_PORTS) {
        fprintf(stderr, "Bad HA port value\n");
        exit(1);
    }
    if (tcam_index < SC_TCAM_RESERVED || tcam_index > 3000) {
        fprintf(stderr, "Bad tcam index value\n");
        exit(1);
    }
    if (stack_size < 1 || stack_size > 4) {
        fprintf(stderr, "Bad stack size value\n");
        exit(1);
    }
    if (nfes < 1 || nfes > MAX_NFE || nfes == 3) {
        fprintf(stderr, "Bad nfes value\n");
        exit(1);
    }
    if (lbg_index < SC_LB_MIN_GROUP || lbg_index > (SC_LB_MAX_GROUP-1)) {
        fprintf(stderr, "Bad load-balance group value\n");
        exit(1);
    }

    /* Pick out unsupported NFE/Stack_size combinations */
    if (nfes < 2 && stack_size > 3) {
        fprintf(stderr, "Bad NFE and stack size combination\n");
        exit(1);
    }

    if (open_scmd_handle(&sd,SC_AP_ID_PORTBYPASS) != 0)
        exit(1);

    if (create_load_balance_groups(&sd, &lbg1, &lbg2, lbg_index, stack_size, nfes) != 0) {
        scmd_close(&sd);
        exit(1);
    }
    scmd_close(&sd);

    if (open_scmd_handle(&sd,SC_AP_ID_TCAM) != 0)
        exit(1);

    if (create_tcam_rules(&sd, ha_port, tcam_index, lbg_index) != 0) {
        scmd_close(&sd);
        exit(1);
    }

    return 0;
}
