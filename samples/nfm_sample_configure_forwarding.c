/**
 * Copyright (C) 2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_configure_forwarding.c
 * Description: NFM sample application to configure forwarding
 *
 * This sample application creates a Layer 2 or Layer 3 forwarding domain
 * and adds logical interfaces to it. The resulting configuration acts
 * as a bridge or a router for untagged Ethernet frames on the specified
 * physical ports.
 */

#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <ns_log.h>
#include <nfm_error.h>
#include <nfm_platform.h>
#include <ns_forwarding.h>
#include <ns_forwarding_error.h>
#include <ns_forwarding_types.h>


static void print_usage(const char* argv0)
{
    fprintf(stderr, "USAGE: %s [options]\n"
                    "\n"
                    "Options:\n"
                    " -l --loglevel n Set log level\n"
                    " -h --help  Display this usage information\n"
                    " -C --no_clear Don't clear config before making changes (default: clear)\n"
                    " -3 --l3    Configure L3 forwarding (default: L2 forwarding)\n"
                    " -i --iface <phyif>:[<vlan>:]<logif>  Map <phyif>(<vlan>) to <logif>\n"
                    " -d --domain <dom>  Create domain with ID <dom>\n"
                    " -b --B1G   Set default mappings for Slot B 4x1G netmod\n"
                    " -m --mtu n  Set MTU for interface(s)\n"
                    " -f --frame-type n Set the frame type for the interface(s)\n"
                    "\n"
                    "Example - configure domain 9 to L3 forward (route) between phy ports 9, 10 and 11:\n"
                    "%s -d9 -3 -i11:1 -i10:2 -i9:3\n",
                    argv0, argv0);
    exit(1);
}

static const struct option __long_options[] = {
    {"no_clear",  0, 0, 'C'},
    {"l3",        0, 0, '3'},
    {"iface",     1, 0, 'i'},
    {"B1G",       0, 0, 'b'},
    {"help",      0, 0, 'h'},
    {"loglevel",  1, 0, 'l'},
    {"domain",    1, 0, 'd'},
    {"mtu",       1, 0, 'm'},
    {"frame-type",1, 0, 'f'},
    {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
    ns_fwd_h fh = NULL;
    ns_nfm_ret_t ret;
    uint32_t domain = 1;
    uint32_t i;
    unsigned int l3_mode = 0;
    unsigned int no_clear = 0;
    uint32_t frame_type = NS_FRAME_TYPE_ETHERNET_II;
    uint32_t mtu = 0;
    int c;
    char *cp, *cp2;
    uint32_t rlim = 20;

    /*
     * Specify the physical interfaces you want to add to the domain. On NMSB
     * systems, the physical port numbers to use can be obtained by invoking
     * the NMSB portstats utility.
     */
    uint32_t phy_intf[NS_FWD_MAX_LOGICAL_INTFS] = { 11, 10,  9,  8 };
    uint32_t log_intf[NS_FWD_MAX_LOGICAL_INTFS] = {  1,  2,  3,  4 };
    uint16_t vlanids[NS_FWD_MAX_LOGICAL_INTFS] =  {  0,  0,  0,  0 };
    uint32_t intf_type;
    uint16_t vlanid;
    uint32_t nphy = 0;
    int defcfg = -1;


    while ((c = getopt_long(argc, argv, "3Cbhi:l:d:m:f:", __long_options, NULL)) != -1) {
        switch (c) {

          case 'i':
            if (defcfg == -1) {
              defcfg = 0;
            } else if (defcfg == 1) {
              fprintf(stderr, "-b and -i are mutually exclusive\n");
              print_usage(argv[0]);
            }
            cp = strchr(optarg, ':');
            if (cp == NULL)
              print_usage(argv[0]);
            *cp++ = '\0';
            phy_intf[nphy] = atoi(optarg);
            cp2 = strchr(cp, ':');
            if (cp2 == NULL) {
                log_intf[nphy++] = atoi(cp);
            } else {
                *cp2++ = '\0';
                vlanids[nphy] = atoi(cp);
                log_intf[nphy++] = atoi(cp2);
            }
            break;

          case 'C':
            no_clear = 1;
            break;

          case '3':
            l3_mode = 1;
            break;

          case 'd':
            domain = atoi(optarg);
            break;

          case 'b':
            if (defcfg == -1) {
              nphy = 4;
              defcfg = 1;
            } else if (defcfg == 0) {
              fprintf(stderr, "-b and -i are mutually exclusive\n");
              print_usage(argv[0]);
            }
            break;

          case 'l':
            ns_log_lvl_set((unsigned int)atoi(optarg));
            break;

          case 'm':
            mtu = atoi(optarg);
            break;

          case 'f':
            frame_type = atoi(optarg);
            break;

          default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (nphy == 0) {
      fprintf(stderr, "No interfaces specified.  Please use -b or -i.\n");
      print_usage(argv[0]);
    }

    /* Obtain a handle to the configuration library */
    printf("Open forwarding configuration subsystem\n");
    ret = ns_fwd_open_platform(&fh);
    if (ret != NS_NFM_SUCCESS) {
        printf("Error 0x%x: %s\n", ret, ns_fwd_error_string(ret));
        return -1;
    }

    if (!no_clear) {
        /* Remove any existing interfaces / bridges / routers */
        printf("Clear existing configuration (if any)\n");
        ret = ns_fwd_clear(fh);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
    }

    /*
     * Configure domain using the variant of the function where the caller specifies
     * the ID. Refer to header file for a variant where the library allocates the ID.
     */
    printf("Configure %s domain with ID %u\n", l3_mode ? "L3" : "L2", domain);
    ret = ns_fwd_configure_domain(fh, domain, l3_mode ? NS_FWD_L3_FORWARDING_DOMAIN : NS_FWD_L2_FORWARDING_DOMAIN);
    if (ret != NS_NFM_SUCCESS)
        goto err_out;

    /* Configure ports, again with the caller specifying the ID */
    for (i = 0; i < nphy; i++) {
        if (vlanids[i] != 0 && vlanids[i] < 0xFFF) {
            intf_type = NS_FWD_C_VLAN_LOG_INTF;
            vlanid = vlanids[i];
        } else {
            intf_type = NS_FWD_UNTAGGED_LOG_INTF;
            vlanid = 0;
        }
        printf("Configure logical interface %u to represent physical port %u\n", log_intf[i], phy_intf[i]);
        ret = ns_fwd_configure_log_intf(fh, log_intf[i], phy_intf[i], intf_type, vlanid);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        printf("Assign logical interface %u to address space ID %u\n",
               log_intf[i], domain);
        ret = ns_fwd_intf_set_addr_space(fh, NS_FWD_LOG_INTF, log_intf[i], domain);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        printf("Attach logical interface %u to domain %u\n", log_intf[i], domain);
        ret = ns_fwd_attach_log_intf(fh, domain, log_intf[i]);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
    }

    /* All cards are reconfigured because ...open_platform was called above */
    printf("Commit changes to the NFE card(s) in the platform\n");
    ret = ns_fwd_commit(fh);
    if (ret != NS_NFM_SUCCESS)
        goto err_out;

    for (i = 0; i < nphy; i++) {
        if (mtu > 0) {
            printf("Setting MTU of lif%u to %u\n", log_intf[i], (unsigned)mtu);
            ret = ns_fwd_intf_set_mtu(fh, NS_FWD_LOG_INTF, log_intf[i], mtu);
            if (ret != NS_NFM_SUCCESS)
                goto err_out;
        }

        if (frame_type != NS_FRAME_TYPE_ETHERNET_II) {
            printf("Setting frame type of lif%u to %u\n", log_intf[i], (unsigned)frame_type);
            ret = ns_fwd_intf_set_frame_type(fh, NS_FWD_LOG_INTF, log_intf[i], frame_type);
            if (ret != NS_NFM_SUCCESS)
                goto err_out;
        }
    }

    if (defcfg == 1) {
        printf("Setting lif%d rate limit to %d\n", log_intf[i], rlim);
        ret = ns_fwd_intf_set_l3ex_ratelim(fh, log_intf[i], rlim);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Reading back lif%d rate limit to verify\n", log_intf[i]);
        rlim = 0;
        ret = ns_fwd_intf_get_l3ex_ratelim(fh, log_intf[i], &rlim);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("L3 exception rate limit for lif%d is %d\n", log_intf[i], rlim);
    }

    /* Close the library handle */
    printf("Close forwarding configuration subsystem\n");
    ret = ns_fwd_close(&fh);
    if (ret != NS_NFM_SUCCESS) {
        printf("Error 0x%x closing forwarding: %s\n", ret, ns_fwd_error_string(ret));
        return -1;
    }

    return 0;

err_out:
    printf("Error 0x%x: %s\n", ret, ns_fwd_error_string(ret));

    ret = ns_fwd_close(&fh);
    if (ret != NS_NFM_SUCCESS)
        printf("Error 0x%x closing forwarding: %s\n", ret, ns_fwd_error_string(ret));

    return -1;
}
