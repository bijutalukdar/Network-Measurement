/**
 * Copyright (C) 2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_l2_static_entry.c
 * Description: NFM sample application to manipulate L2 learning table.
 *
 * This sample adds or removes a L2 learning table static entry.
 */

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

#include <nfm_error.h>
#include <nfm_platform.h>
#include <ns_forwarding.h>
#include <ns_forwarding_error.h>


#define STATIC_ENTRY_ACTION_ADD    0
#define STATIC_ENTRY_ACTION_REMOVE 1


static int valid_mac_address(const char* addr)
{
    if(strlen(addr) != 17)
        return 0;

    if((addr[2] != ':') || (addr[5] != ':') || (addr[8] != ':') || (addr[11] != ':') || (addr[14] != ':') )
       return 0;

    if((!isxdigit((int)addr[0])) ||
       (!isxdigit((int)addr[1])) ||
       (!isxdigit((int)addr[3])) ||
       (!isxdigit((int)addr[4])) ||
       (!isxdigit((int)addr[6])) ||
       (!isxdigit((int)addr[7])) ||
       (!isxdigit((int)addr[9])) ||
       (!isxdigit((int)addr[10])) ||
       (!isxdigit((int)addr[12])) ||
       (!isxdigit((int)addr[13])) ||
       (!isxdigit((int)addr[15])) ||
       (!isxdigit((int)addr[16])) )
       return 0;

    return 1;
}

static void print_usage(const char* argv0)
{
    fprintf(stderr, "USAGE: %s <add/remove> <MAC address> <log intf>\n"
                    "\n"
                    "\tadd           Adds a static entry to the learning table\n"
                    "\tremove        Removes a static entry from the learning table\n"
                    "\tMAC address   The MAC address to add/remove using the format xx:xx:xx:xx:xx:xx\n"
                    "\tlog intf      The logical interface number\n"
                    "\n",
                    argv0);
    exit(1);
}


int main(int argc, char **argv)
{
    ns_fwd_h fh = NULL;
    ns_nfm_ret_t ret;
    uint8_t  mac[6];
    uint32_t  mac_u32[6]; //to avoid warnings
    uint32_t log_intf;
    uint32_t action;
    uint8_t i;

    if(argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    //ACTION:
    if(!strcmp(argv[1], "add")) {
        action = STATIC_ENTRY_ACTION_ADD;
    } else if(!strcmp(argv[1], "remove")) {
        action = STATIC_ENTRY_ACTION_REMOVE;
    } else {
        fprintf(stderr, "\nError : Bad argument %s\n\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    //MAC Address
    if(valid_mac_address(argv[2])) {
        sscanf(argv[2], "%2x:%2x:%2x:%2x:%2x:%2x",
               &mac_u32[0], &mac_u32[1], &mac_u32[2], &mac_u32[3], &mac_u32[4], &mac_u32[5]);
        for(i=0 ; i<6 ; i++) {
            mac[i] = (uint8_t)mac_u32[i];
        }
        //printf("\n\n%x:%x:%x:%x:%x:%x\n\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        fprintf(stderr, "\nError : Invalid MAC address %s\n\n", argv[2]);
        print_usage(argv[0]);
        return 1;
    }

    log_intf = atoi(argv[3]);

    /* Obtain a handle to the configuration library */
    printf("Open forwarding configuration subsystem\n");
    ret = ns_fwd_open_platform(&fh);
    if (ret != NS_NFM_SUCCESS) {
        printf("Error: %s\n", ns_fwd_error_string(ret));
        return -1;
    }


    if(action == STATIC_ENTRY_ACTION_ADD) {
        /* Add an entry to the L2 learning table */
        printf("Add entry to L2 learning table\n");
        ret = ns_fwd_l2_static_entry_add(fh, mac, log_intf);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
    } else {
        /* Remove an entry from the L2 learning table */
        printf("Remove entry from L2 learning table\n");
        ret = ns_fwd_l2_static_entry_remove(fh, mac, log_intf);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
    }

    /* Close the library handle */
    printf("Close forwarding configuration subsystem\n");
    ret = ns_fwd_close(&fh);
    if (ret != NS_NFM_SUCCESS) {
        printf("Error closing forwarding: %s\n", ns_fwd_error_string(ret));
        return -1;
    }

    return 0;

err_out:
    printf("Error: %s\n", ns_fwd_error_string(ret));

    ret = ns_fwd_close(&fh);
    if (ret != NS_NFM_SUCCESS)
        printf("Error closing forwarding: %s\n", ns_fwd_error_string(ret));

    return -1;
}
