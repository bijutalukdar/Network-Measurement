/**
 * Copyright (C) 2012 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_ipsec.c
 * Description: Sample application demonstrating how to efficiently allocate
 *              CPU affinity for IPSecD and its threads in order to allow
 *              maximum via host throughput.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <nfm_error.h>
#include <nfm_platform.h>
#include <ns_ipsec.h>
#include <pthread.h>

#define MAX_THREADS 2+4+4*16 //2 Control + 4 Pivot Threads + 4NFEs*16threads


/* Peg 35 Processor Map
 *
 * Hyperthread 0
 * Sock0   Sock1
 * ------- -------
 * | 0| 1| |10|11|
 * ------- -------
 * | 2| 3| |12|13|
 * ------- -------
 * | 4| 5| |14|15|
 * ------- -------
 * | 6| 7| |16|17|
 * ------- -------
 * | 8| 9| |18|19|
 * ------- -------
 * Hyperthread 1
 * Sock0   Sock1
 * ------- -------
 * |20|21| |30|31|
 * ------- -------
 * |22|23| |32|33|
 * ------- -------
 * |24|25| |34|35|
 * ------- -------
 * |26|27| |36|37|
 * ------- -------
 * |28|29| |38|39|
 * ------- -------
 *
 */

/* Peg30_2U Processor Map
 *
 * Hyperthread 0
 * Sock0   Sock1
 * ------- -------
 * | 0| 2| | 1| 3|
 * ------- -------
 * | 4| 6| | 5| 7|
 * ------- -------
 * | 8|10| | 9|11|
 * ------- -------
 * 
 * Hyperthread 1
 * Sock0   Sock1
 * ------- -------
 * |12|14| |13|15|
 * ------- -------
 * |16|18| |17|19|
 * ------- -------
 * |20|22| |21|23|
 * ------- -------
 *
 */

/* Peg30_1U/Geryon CPU Map
 *
 * Hyperthread 0
 * Sock0
 * -------
 * | 0| 1|
 * -------
 * | 2| 3|
 * -------
 * Hyperthread 1
 * Sock0
 * -------
 * | 4| 5|
 * -------
 * | 6| 7|
 * -------
 *
 */

/* Chrysaor CPU Map
 *
 * Hyperthread 0
 * Sock0
 * -------
 * | 0| 2|
 * -------
 * Hyperthread 1
 * Sock0
 * -------
 * | 1| 3|
 * -------
 *
 */

struct ipsec_opts {
    int msg;
};

static const struct option __long_options[] =
{
    {"help",            0, 0, 'h'},
    {"msg",             1, 0, 'm'},
    {0, 0, 0, 0}
};

static void print_usage (const char* program_name)
{
    fprintf(stderr,
            "USAGE: %s <options> \n"
            "Sample NFM ipsecd application using NFM IPSec API to configure IPSec thread affinity.\n"
            "This app will automatically determine the best thread affinity map given the current\n"
            "platform and number of configured IPSec worker threads.\n"
            "\n"
            "Options:\n"
            " -h --help            Display this usage information\n"
            //" -m --msg             Set to 1 to also adjust NFE msg driver affinity\n"
            //"                      Set to 0 to not adjust NFE msg driver affinity\n"
            //"                      (Default is 1) \n"
            "\n"
            ,
            program_name);
}

static int handle_arg_opts (int argc, char* argv[])
{
    int opt;

    while ((opt=getopt_long(argc, argv, "h", __long_options, NULL)) != -1)
    {
        switch ( opt ) {
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                return -1;
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    ns_nfm_ret_t ret;
    int err, nt, i;
    unsigned int nfes;
    ns_ipsec_h h;
    struct nfe_info_t ni;
    uint64_t mask[MAX_THREADS];

    /* Clear mask */
    for (i=0; i < MAX_THREADS; i++) {
        mask[i] = 0;
    }

    err = handle_arg_opts(argc,argv);
    if (err != 0){
        exit(1);
    }

    /* Open IPSec handle */
    ret = ns_ipsec_open(&h);
    if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "Error obtaining number of NFEs on this system: 0x%x %s:%s\n",
                NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                ns_nfm_error_string(ret));
        exit(1);
    }

    /* Determine number of IPSec worker threads */
    ret = ns_ipsec_get_num_threads(&h, &nt);
    if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "Error obtaining number of IPSec threads : 0x%x %s:%s\n",
                NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                ns_nfm_error_string(ret));
        err = 1;
        goto exit;
    }

    /* Determine number of NFEs */
    ret = nfm_get_nfes_present(&nfes);
    if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "Error obtaining number of NFEs on this system: 0x%x %s:%s\n",
                NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                ns_nfm_error_string(ret));
        err = 1;
        goto exit;
    }

    /* Determine NFE type */
    ret = nfm_get_nfe_info(0, &ni);
    if (ret != NS_NFM_SUCCESS) {
        fprintf(stderr, "Error obtaining  NFE info on this system: 0x%x %s:%s\n",
                NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                ns_nfm_error_string(ret));
        err = 1;
        goto exit;
    }

    /* Decide on thread affinity */
    switch (ni.type) {
        case NFE_TYPE_CAYENNE:
            switch (nfes) {
                case 1:
                    mask[TOFFSET_CONTROL] = 1 << 4;
                    mask[TOFFSET_SPDWORK] = 1 << 4;
                    mask[TOFFSET_PIVOT(nt,0)] = 1 << 4;
                    for (i = 0; i < nt; i++) {
                        mask[TOFFSET_WORKER(nt,0,i)] = 1 << ((i%3)+1);
                    }
                    break;
                case 2:
                    mask[TOFFSET_CONTROL] = 1 << 12;
                    mask[TOFFSET_SPDWORK] = 1 << 12;
                    mask[TOFFSET_PIVOT(nt,0)] = 1 << 12;
                    mask[TOFFSET_PIVOT(nt,1)] = 1 << 13;
                    for (i = 0; i < nt; i++) {
                        mask[TOFFSET_WORKER(nt,0,i)] = 1 << (((i%10)/5)*8+2 + 2*((i%10)%5));
                    }
                    for (i = 0; i < nt; i++) {
                        mask[TOFFSET_WORKER(nt,1,i)] = 1 << (((i%10)/5)*12+3 + 2*((i%10)%5));
                    }
                    break;
            }
            break;
        case NFE_TYPE_HABANERO:
            mask[TOFFSET_CONTROL] = 1 << 20;
            mask[TOFFSET_SPDWORK] = 1 << 20;
            mask[TOFFSET_PIVOT(nt,0)] = 1 << 20;
            mask[TOFFSET_PIVOT(nt,1)] = 1 << 21;
            switch (nfes) {
                case 2:
                    for (i = 0; i < nt; i++)
                        mask[TOFFSET_WORKER(nt,0,i)] = 1 << ((i%9)+1);
                    for (i = 0; i < nt; i++)
                        mask[TOFFSET_WORKER(nt,1,i)] = 1 << ((i%9)+11);
                    break;
                case 4:
                    mask[TOFFSET_PIVOT(nt,2)] = 1 << 30;
                    mask[TOFFSET_PIVOT(nt,3)] = 1 << 31;
                    for (i = 0; i < nt; i++)
                        mask[TOFFSET_WORKER(nt,0,i)] = 1 << ((i%4)+2);
                    for (i = 0; i < nt; i++)
                        mask[TOFFSET_WORKER(nt,1,i)] = 1 << ((i%4)+6);
                    for (i = 0; i < nt; i++)
                        mask[TOFFSET_WORKER(nt,2,i)] = 1 << ((i%4)+12);
                    for (i = 0; i < nt; i++)
                        mask[TOFFSET_WORKER(nt,3,i)] = 1 << ((i%4)+16);
                    break;
            }
            break;
        case NFE_TYPE_GERYON:
            mask[TOFFSET_CONTROL] = 1 << 4;
            mask[TOFFSET_SPDWORK] = 1 << 4;
            mask[TOFFSET_PIVOT(nt,0)] = 1 << 4;
            for (i = 0; i < nt; i++) {
                mask[TOFFSET_WORKER(nt,0,i)] = 1 << ((i%3)+1);
            }
            break;
        case NFE_TYPE_CHRYSAOR:
            mask[TOFFSET_CONTROL] = 1 << 1;
            mask[TOFFSET_SPDWORK] = 1 << 1;
            mask[TOFFSET_PIVOT(nt,0)] = 1 << 1;
            for (i = 0; i < nt; i++)
                mask[TOFFSET_WORKER(nt,0,i)] = 1 << ((i%1)+2);
            break;
        default:
            fprintf(stderr,"Unsupported platform type for setting affinity\n");
            err = 1;
            goto exit;
            break;
    }

    /* Set the thread affinity */
    ret = ns_ipsec_set_cpu_affinity(&h, mask, 2+nfes*(nt+1));
    if (ret == NS_NFM_SUCCESS) {
        printf("Successfully applied Affinity Mask\n");
    } else {
            fprintf(stderr, "Failed to applied Affinity Mask\n");
            fprintf(stderr, "Error 0x%x %s:%s\n",
                        NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                        ns_nfm_error_string(ret));
            err = 1;
    }

exit:
    ns_ipsec_close(&h);
    return err;
}

