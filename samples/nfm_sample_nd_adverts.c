/**
 * Copyright (C) 2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_nd_adverts.c
 * Description: NFM sample application to configure Neighbor Discovery Router Advertisements
 *
 * This sample application configures ND Router Advertisements
 */

#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <ns_log.h>
#include <nfm_error.h>
#include <nfm_platform.h>
#include <ns_forwarding.h>
#include <ns_forwarding_error.h>


static void print_usage(const char* argv0)
{
    fprintf(stderr, "USAGE: %s <options>\n"
                    "\n"
                    "where <options> is one or more of:\n"
                    " -l --loglevel n Set log level\n"
                    " -h --help  Display this usage information\n"
                    " -i --iface <lifN or vifN>\n"
                    " -e Enable adverts\n"
                    " -d Disable adverts\n"
                    " --min_adv_interval <interval seconds>\n"
                    " --max_adv_interval <interval seconds>\n"
                    " --hop_limit <0-255>\n"
                    " --managed <0 or 1>\n"
                    " --other <0 or 1>\n"
                    " --router_lifetime <0 or 1 to 9000 seconds>\n"
                    " --reachable_time <0 to 3600000 milliseconds>\n"
                    " --retransmit_timer <milliseconds>\n"

                    /* Prefix options */
                    " --del_prefix <prefix e.g. 2001::1> [--prefix_len <0 to 128>]\n"
                    " --add_prefix <prefix e.g. 2001::1> [--prefix_len <0 to 128>] [<prefix_options>]\n"
                    "\n"
                    "and where <prefix_options> is one or more of:\n"
                    " --on_link <0 or 1>\n"
                    " --autonomous <0 or 1>\n"
                    " --valid_lifetime <time in seconds>\n"
                    " --preferred_lifetime <time in seconds>\n"
                    "\n"
                    "Example - Enable adverts on lif1 with:\n"
                    "%s -i lif1 -e\n",
                    argv0, argv0);
    exit(1);
}

enum OPT
{
    OPT_MIN_ADV_INTERVAL = 1,
    OPT_MAX_ADV_INTERVAL,
    OPT_HOP_LIMIT,
    OPT_MANAGED,
    OPT_OTHER,
    OPT_ROUTER_LIFETIME,
    OPT_REACHABLE_TIME,
    OPT_RETRANSMIT_TIMER,
    /* Prefix options */
    OPT_ADD_PREFIX,
    OPT_DEL_PREFIX,
    OPT_PREFIX_LEN,
    OPT_ON_LINK,
    OPT_AUTONOMOUS,
    OPT_VALID_LIFETIME,
    OPT_PREF_LIFETIME,
};

static const struct option __long_options[] = {
    {"iface",                   1, 0, 'i' },
    {"help",                    0, 0, 'h' },
    {"loglevel",                1, 0, 'l' },
    {"min_adv_interval",        1, 0, OPT_MIN_ADV_INTERVAL },
    {"max_adv_interval",        1, 0, OPT_MAX_ADV_INTERVAL },
    {"hop_limit",               1, 0, OPT_HOP_LIMIT },
    {"managed",                 1, 0, OPT_MANAGED },
    {"other",                   1, 0, OPT_OTHER },
    {"router_lifetime",         1, 0, OPT_ROUTER_LIFETIME },
    {"reachable_time",          1, 0, OPT_REACHABLE_TIME },
    {"retransmit_timer",        1, 0, OPT_RETRANSMIT_TIMER },
    /* Prefix option */
    {"add_prefix",              1, 0, OPT_ADD_PREFIX },
    {"del_prefix",              1, 0, OPT_DEL_PREFIX },
    {"prefix_len",              1, 0, OPT_PREFIX_LEN },
    {"on_link",                 1, 0, OPT_ON_LINK },
    {"autonomous",              1, 0, OPT_AUTONOMOUS },
    {"valid_lifetime",          1, 0, OPT_VALID_LIFETIME },
    {"preferred_lifetime",      1, 0, OPT_PREF_LIFETIME },
    {0, 0, 0, 0}
};

ns_nfm_ret_t display_prefixes(ns_fwd_h fh, uint32_t intf_type, uint32_t log_or_vir_intf)
{
    ns_nfm_ret_t ret;
    ns_fwd_intf_router_prefix_t *prefix = (ns_fwd_intf_router_prefix_t *)NULL;
    uint32_t num_entries=0;
    uint32_t i;
    char buf[1024];
    size_t buf_size = sizeof(buf);

    /* Call again with prefix null to return number of prefixes defined on interface */
    ret = ns_fwd_intf_nd_advert_get_prefixes(fh, intf_type, log_or_vir_intf, prefix, &num_entries);
    if (ret != NS_NFM_SUCCESS)
        return ret;

    if (num_entries) {
        prefix = (ns_fwd_intf_router_prefix_t *)malloc(sizeof(ns_fwd_intf_router_prefix_t)*num_entries);
        if (!prefix)
            return NS_NFM_OUT_OF_MEMORY;
        /* Call again with prefix non null */
        ret = ns_fwd_intf_nd_advert_get_prefixes(fh, intf_type, log_or_vir_intf, prefix, &num_entries);
        for(i = 0; i < num_entries; i++) {
            printf("%u: %s/%u %u %u %u %u\n", i, inet_ntop(AF_INET6, &prefix[i].prefix, buf, buf_size), prefix[i].len, prefix[i].on_link, prefix[i].autonomous, prefix[i].valid_lifetime, prefix[i].preferred_lifetime);
        }
        free(prefix);
    } else {
        printf("No prefixes\n");
    }
    return ret;
}

int main(int argc, char **argv)
{
    ns_fwd_h fh = NULL;
    ns_nfm_ret_t ret;
    uint32_t i;
    int c;

    int32_t if_type = -1;
    int32_t log_or_vir_intf = -1;
    int32_t enable = -1;
    int64_t min_adv_interval = -1;
    int64_t max_adv_interval = -1;

    int32_t hop_limit = -1;
    int32_t managed = -1;
    int32_t other = -1;
    int64_t router_lifetime = -1;
    int64_t reachable_time = -1;
    int64_t retransmit_timer = -1;

    //struct in6_addr prefix = IN6ADDR_ANY_INIT;
    //
    int32_t add_del_prefix = 0;
    struct in6_addr prefix;
    uint32_t prefix_len = 64;
    uint32_t on_link = 1;
    uint32_t autonomous = 1;
    uint32_t valid_lifetime = 30*24*3600;       /* Default 30 Days in seconds */
    uint32_t preferred_lifetime = 7*24*3600;    /* Default 7 Days in seconds */

    while ((c = getopt_long(argc, argv, "hi:l:ed", __long_options, NULL)) != -1) {
        switch (c) {

            case 'i':
                i = strlen(optarg);
                if (i > 3) {
                    if (strncmp(optarg,"lif",3) == 0) {
                        if_type = NS_FWD_LOG_INTF;
                    } else {
                        if (strncmp(optarg,"vif",3) == 0) {
                            if_type = NS_FWD_VIR_INTF;
                        }
                    }
                    if (if_type != -1) {
                        log_or_vir_intf = atoi(optarg+3);
                    }
                }
                break;

            case 'l':
                ns_log_lvl_set((unsigned int)atoi(optarg));
                break;

            case 'e':
                enable = 1;
                break;

            case 'd':
                enable = 0;
                break;

            case OPT_MIN_ADV_INTERVAL:
                min_adv_interval = atoi(optarg);
                break;

            case OPT_MAX_ADV_INTERVAL:
                max_adv_interval = atoi(optarg);
                break;

            case OPT_HOP_LIMIT:
                hop_limit = atoi(optarg);
                break;

            case OPT_MANAGED:
                managed = atoi(optarg);
                break;

            case OPT_OTHER:
                other = atoi(optarg);
                break;

            case OPT_ROUTER_LIFETIME:
                router_lifetime = atoi(optarg);
                break;

            case OPT_REACHABLE_TIME:
                reachable_time = atoi(optarg);
                break;

            case OPT_RETRANSMIT_TIMER:
                retransmit_timer = atoi(optarg);
                break;

            case OPT_ADD_PREFIX:
                add_del_prefix++;
                if (inet_pton(AF_INET6, optarg, (void*)&prefix) != 1)
                {
                    printf("Error parsing prefix\n");
                    return -1;
                }
                break;

            case OPT_DEL_PREFIX:
                add_del_prefix--;
                if (inet_pton(AF_INET6, optarg, (void*)&prefix) != 1)
                {
                    printf("Error parsing prefix\n");
                    return -1;
                }
                break;

            case OPT_PREFIX_LEN:
                prefix_len = atoi(optarg);
                break;

            case OPT_ON_LINK:
                on_link = atoi(optarg);
                break;

            case OPT_AUTONOMOUS:
                autonomous = atoi(optarg);
                break;

            case OPT_VALID_LIFETIME:
                valid_lifetime = atoi(optarg);
                break;

            case OPT_PREF_LIFETIME:
                preferred_lifetime = atoi(optarg);
                break;

            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (log_or_vir_intf < 0) {
        fprintf(stderr, "No interfaces specified.  Please use -i.\n");
        print_usage(argv[0]);
    }

    /* Obtain a handle to the configuration library */
    printf("Open forwarding configuration subsystem\n");
    ret = ns_fwd_open_platform(&fh);
    if (ret != NS_NFM_SUCCESS) {
        printf("Error: %s\n", ns_fwd_error_string(ret));
        return -1;
    }

    if (enable != -1) {
        uint32_t rb_enabled;
        ret = ns_fwd_intf_nd_advert_enable(fh, if_type, log_or_vir_intf, enable);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_is_enabled(fh, if_type, log_or_vir_intf, &rb_enabled);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Adverts are %s\n", rb_enabled ? "enabled" : "disabled");
    }

    if ((min_adv_interval != -1) && (max_adv_interval != -1)) {
        uint32_t rb_min_adv_interval;
        uint32_t rb_max_adv_interval;
        ret = ns_fwd_intf_nd_advert_set_min_max_rtr_adv_interval(fh, if_type, log_or_vir_intf, (uint32_t)min_adv_interval, (uint32_t)max_adv_interval);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_get_min_max_rtr_adv_interval(fh, if_type, log_or_vir_intf, &rb_min_adv_interval, &rb_max_adv_interval);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("min_adv_interval: %u max_adv_interval: %u\n", rb_min_adv_interval, rb_max_adv_interval);
    } else if ((min_adv_interval != -1) || (max_adv_interval != -1)) {
        printf("Please specify both min_adv_interval and max_adv_interval options\n");
        return -1;
    }

    if (hop_limit != -1)
    {
        uint32_t rb_hop_limit;
        ret = ns_fwd_intf_nd_advert_set_hoplimit(fh, if_type, log_or_vir_intf, (uint32_t)hop_limit);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_get_hoplimit(fh, if_type, log_or_vir_intf, &rb_hop_limit);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Hop limit: %u\n", rb_hop_limit);
    }

    if (managed != -1)
    {
        uint32_t rb_managed;
        ret = ns_fwd_intf_nd_advert_set_managed(fh, if_type, log_or_vir_intf, (uint32_t)managed);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_get_managed(fh, if_type, log_or_vir_intf, &rb_managed);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Managed: %u\n", rb_managed);

    }

    if (other != -1)
    {
        uint32_t rb_other;
        ret = ns_fwd_intf_nd_advert_set_other(fh, if_type, log_or_vir_intf, (uint32_t)other);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_get_other(fh, if_type, log_or_vir_intf, &rb_other);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Other: %u\n", rb_other);
    }

    if (router_lifetime != -1)
    {
        uint32_t rb_router_lifetime;
        ret = ns_fwd_intf_nd_advert_set_router_lifetime(fh, if_type, log_or_vir_intf, (uint32_t)router_lifetime);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_get_router_lifetime(fh, if_type, log_or_vir_intf, &rb_router_lifetime);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Router lifetime: %u\n", rb_router_lifetime);
    }

    if (reachable_time != -1)
    {
        uint32_t rb_reachable_time;
        ret = ns_fwd_intf_nd_advert_set_reachable_time(fh, if_type, log_or_vir_intf, (uint32_t)reachable_time);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_get_reachable_time(fh, if_type, log_or_vir_intf, &rb_reachable_time);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Reachable time: %u\n", rb_reachable_time);
    }

    if (retransmit_timer != -1)
    {
        uint32_t rb_retransmit_timer;
        ret = ns_fwd_intf_nd_advert_set_retransmit_timer(fh, if_type, log_or_vir_intf, (uint32_t)retransmit_timer);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        /* Read back the setting */
        ret = ns_fwd_intf_nd_advert_get_retransmit_timer(fh, if_type, log_or_vir_intf, &rb_retransmit_timer);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
        printf("Retransmit timer: %u\n", rb_retransmit_timer);
    }

    if (add_del_prefix == 1)
    {
        char buf[1024];
        size_t buf_size = sizeof(buf);

        printf("Adding prefix: %s/%d\n", inet_ntop(AF_INET6, &prefix, buf, buf_size),prefix_len);
        ret = ns_fwd_intf_nd_advert_add_prefix(fh, if_type, log_or_vir_intf, &prefix, prefix_len, on_link, autonomous, valid_lifetime, preferred_lifetime);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        ret = display_prefixes(fh, if_type, log_or_vir_intf);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;
    }

    if (add_del_prefix == -1)
    {
        char buf[1024];
        size_t buf_size = sizeof(buf);

        printf("Deleting prefix: %s/%d\n", inet_ntop(AF_INET6, &prefix, buf, buf_size),prefix_len);
        ret = ns_fwd_intf_nd_advert_del_prefix(fh, if_type, log_or_vir_intf, &prefix, prefix_len);
        if (ret != NS_NFM_SUCCESS)
            goto err_out;

        ret = display_prefixes(fh, if_type, log_or_vir_intf);
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
