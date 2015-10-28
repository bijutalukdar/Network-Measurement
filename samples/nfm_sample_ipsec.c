/**
 * Copyright (C) 2012 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_ipsec.c
 * Description: Sample application demonstrating the use of ipsec counters.
 */

/**
 * TODO:
 *   - print names when displaying TIDs
 *   - allow user to set CPU affinity for any thread
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <nfm_error.h>
#include <ns_ipsec.h>
#include <pthread.h>

#include "ns_cntr_names.h"

#define DECLARE_COUNTER(x) [x] = #x,
#define DECLARE_COUNTER_AT(x,y) DECLARE_COUNTER(x)

#define ARR_LEN(x)      (sizeof(x) / sizeof(x[0]))

char * ns_ipsec_cntrs[] = {
#include "ns_cntr_ipsec.cntr"
};
#undef DECLARE_COUNTER
#undef DECLARE_COUNTER_AT

#include <ns_cntr.h>

struct ipsec_opts {
    int global_cntr;
    int vnet_cntr;
    int tids;
    uint64_t aff_mask;
};

static struct ipsec_opts opts = {-1, -1, -1, 0};

static const struct option __long_options[] =
{
    {"global-cntrs",    0, 0, 'g'},
    {"vnet-cntrs",      1, 0, 'v'},
    {"tids",            0, 0, 't'},
    {"cpu-aff",         1, 0, 'c'},
    {"help",            0, 0, 'h'},
    {0, 0, 0, 0}
};

static void print_usage (const char* program_name)
{
    fprintf(stderr,
            "USAGE: %s <options> \n"
            "Sample NFM ipsecd application using NFM IPSec API\n"
            "\n"
            "Options:\n"
            " -h --help            Display this usage information\n"
            " -g --global-cntrs    Display global ipsecd counters\n"
            " -v --vnet-cntrs=vnet Display ipsecd counters for a VNet(VR)\n"
            " -t --tids            Display the pids for ipsecd threads.\n"
            " -c --cpu-aff=mask    Set CPU affinity of control thread\n"
            "                      (Use API to set affinity of other threads\n"
            "                       `taskset -p <pid>' will get CPU affinity)\n"
            "\n"
            ,
            program_name);
}

static int handle_arg_opts (int argc, char* argv[])
{
    int opt;
    char *err_opts;

    if (argc == 1)
        goto handle_err;

    while ((opt=getopt_long(argc, argv, "hgv:tc:", __long_options, NULL)) != -1)
    {
        switch ( opt ) {
            case 'g':
                opts.global_cntr = 1;
                break;
            case 'v':
                opts.vnet_cntr = strtol(optarg, &err_opts, 0);
                if ( *err_opts != '\0' )
                    goto handle_strtol_err;
                break;
            case 't':
                opts.tids = 1;
                break;
            case 'c':
                opts.aff_mask = strtoull(optarg, &err_opts, 0);
                if ( *err_opts != '\0' )
                    goto handle_strtol_err;
                break;
            case 'h':
                print_usage(argv[0]);
                //fall through
            default:
                return -1;
        }
    }

    return 0;

handle_strtol_err:
    fprintf(stderr, "Error, invalid arguments %s for flag -%c\n", err_opts,
    opt);
handle_err:
    fprintf(stderr,
            "USAGE: %s [options] \n", argv[0]);
    return -1;

}

int main(int argc, char* argv[])
{
    ns_ipsec_h h;
    int err;
    ns_nfm_ret_t ret;

    uint64_t cntr;
    uint32_t i;
    int ntids;
    pid_t tids[1024];

    err = handle_arg_opts(argc,argv);
    if (err != 0){
        exit(1);
    }

    ret = ns_ipsec_open(&h);

    if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "error: ipsec handle open failed - %s\n",
                                            ns_nfm_error_string(ret));
	return -1;
    }

    if (opts.global_cntr != -1) {
        for (i=0; i < NS_CNTR_IPSEC_LAST; i++) {
            ret = ns_cntr_read_ipsec_global(i, &cntr);
            if (ret == NS_NFM_SUCCESS) {
                printf("%2d: %s = %ld\n", i, ns_ipsec_cntrs[i], cntr);
            } else {
                fprintf(stderr, "global counters error 0x%x %s:%s\n",
                        NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                        ns_nfm_error_string(ret));
            }
        }
    }

    if (opts.vnet_cntr != -1) {
        printf("VR %d counters\n", opts.vnet_cntr);
        for (i=0; i < NS_CNTR_IPSEC_LAST; i++) {
            ret = ns_cntr_read_ipsec_vr(opts.vnet_cntr, i, &cntr);
            if (ret == NS_NFM_SUCCESS) {
                printf("%2d: %s = %ld\n", i, ns_ipsec_cntrs[i], cntr);
            } else {
                fprintf(stderr, "VR counters error 0x%x %s:%s\n",
                        NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                        ns_nfm_error_string(ret));
            }
        }
        printf("\n");
    }

    if (opts.tids != -1) {
        printf("TIDs\n");
        ret = ns_ipsec_get_tid(&h, tids, 1024, &ntids);
        if (ret == NS_NFM_SUCCESS) {
            for (i=0; i < (uint32_t)ntids; i++) {
                printf("TIDs[%d] = %u\n", i, tids[i]);
            }
        } else {
                fprintf(stderr, "Error 0x%x %s:%s\n",
                        NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                        ns_nfm_error_string(ret));
        }
        printf("\n");
    }

    if (opts.aff_mask != 0) {
        printf("Setting control thread affinity to 0x%lX ...\n", opts.aff_mask);
        ret = ns_ipsec_set_cpu_affinity(&h, &opts.aff_mask, 1);
        if (ret == NS_NFM_SUCCESS) {
            printf("Successfully applied Affinity Mask\n");
        } else {
            fprintf(stderr, "Failed to applied Affinity Mask\n");
            fprintf(stderr, "Error 0x%x %s:%s\n",
                        NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                        ns_nfm_error_string(ret));
        }
        printf("\n");
    }

    ns_ipsec_close(&h);

    return 0;
}

