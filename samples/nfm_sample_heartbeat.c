/**
 * Copyright (C) 2011-2013 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_heartbeat.c
 * Description: Sample application using NFM Stacking API to generate
                heartbeat traffic.
 */

#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nfm_error.h>
#include <ns_clustering.h>
#include <ns_forwarding_error.h>

#define USEC_PER_MSEC                   1000
#define USEC_PER_SEC                    1000 * USEC_PER_MSEC
#define NSEC_PER_MSEC                   1000 * USEC_PER_MSEC

#define DEFAULT_HEARTBEAT_COUNT         3
#define DEFAULT_TIMEOUT                 1000 * USEC_PER_MSEC
#define DEFAULT_DST_NFE                 0
#define DEFAULT_FINGERPRINT             0
#define DEFAULT_DATESTAMP               0
#define DEFAULT_BROADCAST               0
#define INVALID_DST_STACK               -1

unsigned int round_trip_time, highest_rrt, lowest_rrt, heartbeat_acks;
int loss;
unsigned long long int acc_rtt;
unsigned int i, hbs;
time_t starttime, endtime;

struct heartbeat_options{
    int cluster_id;
    int nfe_id;
    int timeout;
    int datestamp;
    int broadcast;
    unsigned int count;
    unsigned int fingerprint;
};

static const struct option __long_options[] = {
    {"nfe",             1, 0, 'n'},
    {"timeout",         1, 0, 't'},
    {"count",           1, 0, 'c'},
    {"fingerprint",     1, 0, 'f'},
    {"datestamp",       0, 0, 'd'},
    {"broadcast",       0, 0, 'b'},
    {"help",            0, 0, 'h'},
    {0, 0, 0, 0}
};

static void print_usage (const char* program_name)
{
    fprintf(stderr,
            "USAGE: %s [options] dst_cluster_id \n"
            "Sample application using NFM Stacking API to send\n"
            "a heartbeat request to the specified cluster ID.\n"
            "\n"
            "Options:\n"
            " -h --help           Display this usage information\n"
            " -n --nfe=ID         Set the destination NFE [0-1] (Default %u)\n"
            " -c --count=NUM      The number of heartbeat request to be sent.\n"
            "                      Use -1 to specify infinite (Default %u)\n"
            " -t --timeout=TIME   The timeout in microseconds (Default %u)\n"
            " -f --fingerprint=FP Set a fingerprint (Default %u)\n"
            " -d --datestamp      Timestamp the output\n"
            " -b --broadcast      Send heartbeat broadcast\n"
            "\n"
            "Example - send 5 heartbeat requests to cluster ID 2 NFE 1 with a\n"
            "10,000 usec timeout:\n"
            "%s -c5 -n1 -t10000 2\n\n",
            program_name, DEFAULT_DST_NFE, DEFAULT_HEARTBEAT_COUNT,
            DEFAULT_TIMEOUT, DEFAULT_FINGERPRINT,
            program_name);
}

static int handle_arg_opts (int argc, char* argv[], struct heartbeat_options *hb_opts)
{
    int opt;
    char *err_opts;

    while ( (opt = getopt_long(argc, argv, "n:t:c:f:dbh", __long_options, NULL)) != -1 ) {
        switch ( opt ) {
            case 'n':
                hb_opts->nfe_id = strtol(optarg, &err_opts, 0);
                if ( *err_opts != '\0' )
                    goto handle_strtol_err;
                break;

            case 'c':
                hb_opts->count = strtoul(optarg, &err_opts, 0);
                if ( *err_opts != '\0' )
                    goto handle_strtol_err;
                break;

            case 't':
                hb_opts->timeout = strtol(optarg, &err_opts, 0);
                if ( *err_opts != '\0' )
                    goto handle_strtol_err;
                break;

            case 'f':
                hb_opts->fingerprint = strtoul(optarg, &err_opts, 0);
                if ( *err_opts != '\0' )
                    goto handle_strtol_err;
                break;

            case 'd':
                hb_opts->datestamp = 1;
                break;

            case 'b':
                hb_opts->broadcast = 1;
                break;

            case 'h':
                print_usage(argv[0]);
                //fall through
            default:
                return -1;
        }
    }

    if ( hb_opts->broadcast )
        return 0;
    
    if ( hb_opts->cluster_id == -1 && (optind == argc) )
        goto handle_err;

    hb_opts->cluster_id = strtol(argv[optind], &err_opts, 0);
    if ( *err_opts != '\0' )
        goto handle_strtol_err;

    return 0;

handle_strtol_err:
    fprintf(stderr, "Error, invalid arguments %s for flag -%c\n", err_opts,
    opt);
handle_err:
    fprintf(stderr,
            "USAGE: %s [options] dst_cluster_id \n", argv[0]);
    return -1;

}

void heartbeat_result(int signum)
{
    if (signum != SIGINT && signum != 0)
        exit(1);

    time(&endtime);
    loss = hbs-heartbeat_acks;
    if (loss < 0) loss = 0; // hbs is incremented after heartbeat_ack, so can be < 0

    if (heartbeat_acks > 0) {
        printf("\nReceived %u heartbeat ACKs, total time %dsec\n"
                "avg RTT %llu.%03llu msec, max RTT %u.%03u msec, min RTT %u.%03u msec\n",
               heartbeat_acks, (int)(endtime-starttime),
               (acc_rtt / heartbeat_acks) / USEC_PER_MSEC,
               (acc_rtt / heartbeat_acks) % USEC_PER_MSEC,
               highest_rrt / USEC_PER_MSEC,
               highest_rrt % USEC_PER_MSEC,
               lowest_rrt / USEC_PER_MSEC,
               lowest_rrt % USEC_PER_MSEC);
        printf("Packets lost %u of %u (%u%%)\n", loss, hbs, (100 * loss) / hbs);
    }
    printf("Done.\n");

    exit(loss);
}

static void send_heartbeat(struct heartbeat_options *hb_opts)
{
    time_t rawtime;
    struct tm *tzname;
    printf("Sending heartbeat to appliance %d, NFE %d (timeout %u.%03u msec)..."
            "\n", hb_opts->cluster_id, hb_opts->nfe_id,
            hb_opts->timeout / USEC_PER_MSEC, hb_opts->timeout % USEC_PER_MSEC);
    while (i < hb_opts->count) {
        ns_nfm_ret_t ret = ns_cluster_heartbeat_send(hb_opts->nfe_id,
                hb_opts->cluster_id, hb_opts->nfe_id, hb_opts->timeout,
                hb_opts->fingerprint, &round_trip_time);
        time(&rawtime);
        tzname = localtime(&rawtime);
        if (hb_opts->datestamp)
          printf("%02d:%02d:%02d ", tzname->tm_hour, tzname->tm_min, tzname->tm_sec);
        if ( ret != NS_NFM_SUCCESS ) {
            printf("Error 0x%x %s:%s\n",
                    NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                    ns_nfm_error_string(ret));
        } else {
            printf("received ACK: heartbeat seq %u, RTT: %u.%03u msec\n",
                    i, round_trip_time / USEC_PER_MSEC,
                    round_trip_time % USEC_PER_MSEC);
            if (round_trip_time > highest_rrt)
                highest_rrt = round_trip_time;
            if (round_trip_time < lowest_rrt)
                lowest_rrt = round_trip_time;
            acc_rtt += round_trip_time;
            heartbeat_acks++;
        }
        i++;
        hbs++;
        sleep(1);
    }
}

static void send_heartbeat_broadcast(struct heartbeat_options *hb_opts)
{
    time_t rawtime;
    struct tm *tzname;
    ns_cluster_hb_status_t hb_s;
    ns_nfm_ret_t ret;
    int j, k, l;
    hb_opts->fingerprint = 0x100;

    bzero(&hb_s, sizeof(ns_cluster_hb_status_t));

    printf("Sending heartbeat broadcast (timeout %u.%03u msec)...\n",
            hb_opts->timeout / USEC_PER_MSEC, hb_opts->timeout % USEC_PER_MSEC);
    while (i < hb_opts->count) {
        ret = ns_cluster_heartbeat_broadcast(hb_opts->timeout,
                                             hb_opts->fingerprint,
                                             &hb_s);
        time(&rawtime);
        tzname = localtime(&rawtime);
        printf("\n%02d:%02d:%02d\n", tzname->tm_hour, tzname->tm_min, tzname->tm_sec);
        if ( ret != NS_NFM_SUCCESS ) {
            printf("Error 0x%x %s:%s\n",
                    NS_NFM_ERROR_CODE(ret), ns_nfm_module_string(ret),
                    ns_nfm_error_string(ret));
        }

        for (j = 0; j < MAX_CLUSTER_SIZE; j++) {
            for (k = 0; k < MAX_NFES; k++) {
                for (l = 0; l < MAX_NFES; l++) {
                    if (hb_s.hb_dest[j].round_trip_times[k][l] == 0)
                        continue;

                    hbs++;

                    if (hb_s.hb_dest[j].round_trip_times[k][l] < 0) {
                        printf("DST_APP %d SRC_NFE %d DST_NFE %d = FAIL\n", j, k, l);
                        continue;
                    }
                    round_trip_time = hb_s.hb_dest[j].round_trip_times[k][l];

                    if (round_trip_time > highest_rrt)
                        highest_rrt = round_trip_time;
                    if (round_trip_time < lowest_rrt)
                        lowest_rrt = round_trip_time;
                    acc_rtt += round_trip_time;
                    heartbeat_acks++;

                    printf("DST_APP %d SRC_NFE %d DST_NFE %d = %d.%03d msec\n", j, k, l,
                           round_trip_time / USEC_PER_MSEC, round_trip_time % USEC_PER_MSEC);
                }
            }
        }

        i++;
        sleep(1);
    }
}

int main (int argc, char* argv[])
{
    int err;
    struct heartbeat_options hb_opts = {INVALID_DST_STACK, DEFAULT_DST_NFE,
        DEFAULT_TIMEOUT, DEFAULT_DATESTAMP, DEFAULT_BROADCAST,
        DEFAULT_HEARTBEAT_COUNT, DEFAULT_FINGERPRINT};
    lowest_rrt = DEFAULT_TIMEOUT;

    // handle user options
    err = handle_arg_opts(argc,argv,&hb_opts);
    if (err != 0)
        exit(1);

    signal(SIGINT, heartbeat_result); // register SIGINT for ctl-c

    // record start time used to calculate total runtime in heartbeat_result()
    time(&starttime);

    if (hb_opts.broadcast)
        send_heartbeat_broadcast(&hb_opts);
    else
        send_heartbeat(&hb_opts);

    heartbeat_result(0);

    return loss;
}
