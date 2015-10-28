/**
 * Copyright (C) 2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_neigh_entries.c
 * Description: NFM sample application to display resolved neighbor table entries.
 *
 * This sample displays resolved neighbor table entries.
 */

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

#include <nfm_error.h>
#include <nfm_platform.h>
#include <ns_forwarding.h>
#include <ns_forwarding_error.h>

#define MAX_ENTRIES 64*1024

ns_fwd_neigh_entry_t entries[MAX_ENTRIES];

int main(int argc, char **argv)
{
    ns_fwd_h fh = NULL;
    ns_nfm_ret_t ret;
    uint32_t num_entries, i;
    char ip_as_str[1024];

    if(argc != 1) {
        printf("usage: %s\n", argv[0]);
        return 1;
    }

    /* Obtain a handle to the configuration library */
    ret = ns_fwd_open_platform(&fh);
    if (ret != NS_NFM_SUCCESS) {
        printf("Error: %s\n", ns_fwd_error_string(ret));
        return -1;
    }

    /* Add an entry to the L2 learning table */
    num_entries = MAX_ENTRIES;
    ret = ns_fwd_neigh_get_entries(fh, NS_FWD_VR_ALL, entries, &num_entries);
    if (ret != NS_NFM_SUCCESS)
        goto err_out;

    /* Close the library handle */
    ret = ns_fwd_close(&fh);
    if (ret != NS_NFM_SUCCESS) {
        printf("Error closing forwarding: %s\n", ns_fwd_error_string(ret));
        return -1;
    }

    /* Display data */
    for (i = 0; i < num_entries; i++) {
        if (entries[i].resolved) {
            printf("vr%u %s %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx lif%u\n",
                   entries[i].vrid,
                   inet_ntop(entries[i].ip_af, entries[i].ip_addr.buf, ip_as_str, sizeof(ip_as_str)),
                   entries[i].mac_addr[0], entries[i].mac_addr[1], entries[i].mac_addr[2],
                   entries[i].mac_addr[3], entries[i].mac_addr[4], entries[i].mac_addr[5],
                   entries[i].log_intf);
        }
    }
    return 0;

err_out:
    printf("Error: %s\n", ns_fwd_error_string(ret));

    ret = ns_fwd_close(&fh);
    if (ret != NS_NFM_SUCCESS)
        printf("Error closing forwarding: %s\n", ns_fwd_error_string(ret));

    return -1;
}
