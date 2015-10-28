/*
 * Copyright (C) 2012 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_peg35_nmsb_config.c
 * Description: Sample application to configure the NMSB to have identical
 *              load-balancing trunk groups, such that each such that each IO
 *              port will load-balance to effectively the same NFE. This is
 *              to be done when Pegasus 3.5 platforms have their packet flipping
 *              feature turned off.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include "ns_log.h"
#include "scmd.h"

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s \n"
                  "Configure the NMSB to have identical load-balancing trunk groups,\n"
                  "such that each IO port will load-balance to effectively the same\n"
                  "table. i.e. Table1's contents are written to Table2.\n"
                  "To reverse this change, reconfigure the NMSB using modeset.\n",
          argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"help",      0, 0, 'h'},
  {0, 0, 0, 0}
};

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

int main(int argc, char *argv[])
{
    struct sc_desc sd;
    struct sc_lb_group lbg1;
    struct sc_lb_group lbg2;
    struct sc_lb_group lbgx;
    int rc;

    int c;
    while ((c = getopt_long(argc, argv, "h", __long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
            default:
                print_usage(argv[0]);
        }
    }

    if (open_scmd_handle(&sd,SC_AP_ID_PORTBYPASS) != 0)
        exit(1);

    memset(&lbg1, 0, sizeof(struct sc_lb_group));

    /* Read lbg1*/
    lbg1.index = 1;
    rc = scmd_read_lb_group(&sd, &lbg1);
    if (rc != 0) {
        fprintf(stderr, "Error reading load-balance group 1: %s\n", scmd_strerror(rc));
        scmd_close(&sd);
        exit(1);
    }

    /* Now overwrite lbg2 */
    memcpy(&lbg2,&lbg1,sizeof(struct sc_lb_group));
    lbg2.index = 2;
    rc = scmd_set_lb_group(&sd, &lbg2);
    if (rc != 0) {
        fprintf(stderr, "Error setting load-balance group 2: %s\n", scmd_strerror(rc));
        scmd_close(&sd);
        exit(1);
    }

    /* Now read back lbg2 */
    memset(&lbgx,0,sizeof(struct sc_lb_group));
    lbgx.index = 2;
    rc = scmd_read_lb_group(&sd, &lbgx);
    if (rc != 0) {
        fprintf(stderr, "Error reading load-balance group 2: %s\n", scmd_strerror(rc));
        scmd_close(&sd);
        exit(1);
    }

    /* Now compare the read back lbg to what we wrote */
    if (memcmp(&lbgx, &lbg2, sizeof(struct sc_lb_group)) != 0) {
        fprintf(stderr, "Error setting load-balance group 1 to equal 2\n");
        scmd_close(&sd);
        exit(1);
    }

    scmd_close(&sd);
    return 0;
}
