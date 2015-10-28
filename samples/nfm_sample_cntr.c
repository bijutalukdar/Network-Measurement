
//-------------------------------------------------------------------------
// Copyright (C) 2007-2011 Netronome Systems, Inc.  All rights reserved.
//-------------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>

#include "ns_cntr.h"
#include "ns_log.h"

/* This should really be extracted automatically at run-time. */
#define NS_MSG_MAX_HOST_DEST 32

#define MAX_NFE_PORTS        2
#define COUNTERS_PER_PORT    8

#define FORMAT_COUNTER(c) \
    printf("%-50s%12lu\n", #c, (unsigned long)raw)
#define FORMAT_COUNTER_S(s) \
    printf("%-50s%12lu\n", s, (unsigned long)raw)
#define PRINT_COUNTER(c) \
    if (wrap_ns_cntr(msg_h, c, &raw))                 \
        return 1;                                     \
    printf("%-50s%12lu\n", #c, (unsigned long)raw)



char RxPortStats[32][50] = {"CNTR_DEBUG_PORT_0_PACKET_PKTS_RECEIVED", \
                            "CNTR_DEBUG_PORT_0_PACKET_PKTS_DROPPED",    \
                            "CNTR_DEBUG_PORT_0_PACKET_PKTS_RING_FULL_DROPS", \
                            "CNTR_DEBUG_PORT_0_PACKET_BYTES_RECEIVED",  \
                            "CNTR_DEBUG_PORT_0_PACKET_PKTS_DROPPED_SPP", \
                            "CNTR_DEBUG_PORT_0_PACKET_PKTS_DROPPED_SEP", \
                            "CNTR_DEBUG_PORT_0_PACKET_PKTS_DROPPED_RSW", \
                            "CNTR_DEBUG_PORT_0_PACKET_PKTS_DROPPED_MSP_ESP", \
                            "CNTR_DEBUG_PORT_1_PACKET_PKTS_RECEIVED",   \
                            "CNTR_DEBUG_PORT_1_PACKET_PKTS_DROPPED",    \
                            "CNTR_DEBUG_PORT_1_PACKET_PKTS_RING_FULL_DROPS", \
                            "CNTR_DEBUG_PORT_1_PACKET_BYTES_RECEIVED",  \
                            "CNTR_DEBUG_PORT_1_PACKET_PKTS_DROPPED_SPP", \
                            "CNTR_DEBUG_PORT_1_PACKET_PKTS_DROPPED_SEP", \
                            "CNTR_DEBUG_PORT_1_PACKET_PKTS_DROPPED_RSW", \
                            "CNTR_DEBUG_PORT_1_PACKET_PKTS_DROPPED_MSP_ESP", \
                            "CNTR_DEBUG_PORT_2_PACKET_PKTS_RECEIVED",   \
                            "CNTR_DEBUG_PORT_2_PACKET_PKTS_DROPPED",    \
                            "CNTR_DEBUG_PORT_2_PACKET_PKTS_RING_FULL_DROPS", \
                            "CNTR_DEBUG_PORT_2_PACKET_BYTES_RECEIVED",  \
                            "CNTR_DEBUG_PORT_2_PACKET_PKTS_DROPPED_SPP", \
                            "CNTR_DEBUG_PORT_2_PACKET_PKTS_DROPPED_SEP", \
                            "CNTR_DEBUG_PORT_2_PACKET_PKTS_DROPPED_RSW", \
                            "CNTR_DEBUG_PORT_2_PACKET_PKTS_DROPPED_MSP_ESP", \
                            "CNTR_DEBUG_PORT_3_PACKET_PKTS_RECEIVED",   \
                            "CNTR_DEBUG_PORT_3_PACKET_PKTS_DROPPED",    \
                            "CNTR_DEBUG_PORT_3_PACKET_PKTS_RING_FULL_DROPS", \
                            "CNTR_DEBUG_PORT_3_PACKET_BYTES_RECEIVED",  \
                            "CNTR_DEBUG_PORT_3_PACKET_PKTS_DROPPED_SPP", \
                            "CNTR_DEBUG_PORT_3_PACKET_PKTS_DROPPED_SEP", \
                            "CNTR_DEBUG_PORT_3_PACKET_PKTS_DROPPED_RSW", \
                            "CNTR_DEBUG_PORT_3_PACKET_PKTS_DROPPED_MSP_ESP"
                           };


static ns_cntr_h msg_h = 0;

//-------------------------------------------------------------------------
static void atexit_func()
{
    if (msg_h)
        ns_cntr_shutdown_messaging(msg_h);
}
//-------------------------------------------------------------------------
ns_nfm_ret_t wrap_ns_cntr(ns_cntr_h msg_h, unsigned int index,
                          uint64_t *raw_count)
{
    ns_nfm_ret_t ret;

    ret = ns_cntr(msg_h, NS_CNTR_READ, index, raw_count);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS)
    {
        NS_LOG_ERROR("ns_cntr(%u,%u) (%d,%d): %s",
                     NS_CNTR_READ,
                     index,
                     NS_NFM_ERROR_CODE(ret),
                     NS_NFM_ERROR_SUBCODE(ret),
                     ns_nfm_error_string(ret));
    }
    return ret;
}
//-------------------------------------------------------------------------
void show_usage(void)
{
    printf("nfm_sample_cntr [options]\n");
    printf("options:\n");
    printf("  -d #   Specify NFE device, default 0\n");
    exit(1);
}
//-------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    unsigned int card_id;
    unsigned int seq_no;
    unsigned int index;
    ns_nfm_ret_t ret;
    int i, j;
    uint64_t raw;
    uint64_t cntrs[CNTRTAB_NUM_ROWS];
    int32_t num_cntrs;

    card_id = 0;
    if (argc > 1) {
        if (argv[1][0] == '-') {
            if (argv[1][1] != 'd') {
                show_usage();
                return 1;
            }
            if (argc < 3) {
                show_usage();
                return 1;
            }
            for (i = 0; argv[2][i]; i++) {
                if (argv[2][i] < '0') break;
                if (argv[2][i] > '9') break;
            }
            if (argv[2][i]) {
                show_usage();
                return 1;
            }
            card_id = atoi(argv[2]);
        }
    }
    printf("Using NFE%u\n",card_id);

    ns_log_init(NS_LOG_CONSOLE | NS_LOG_COLOR);
    ns_log_lvl_set(NS_LOG_LVL_INFO);

    ret = ns_cntr_init_messaging(&msg_h,card_id);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
        NS_LOG_ERROR("ns_cntr_init_messaging (%d,%d): %s",
                     NS_NFM_ERROR_CODE(ret),
                     NS_NFM_ERROR_SUBCODE(ret),
                     ns_nfm_error_string(ret));
        return 1;
    }
    atexit(atexit_func);

    printf("%-50s%12s\n", " ", "total");
    PRINT_COUNTER(CNTR_DEBUG_CNT_DL_DROPS);
    PRINT_COUNTER(CNTR_DEBUG_CNT_RULE_DROPS);
    PRINT_COUNTER(CNTR_DEBUG_CNT_RULE_DROP_NOTIFY);
    PRINT_COUNTER(CNTR_DEBUG_CNT_RX_TO_NFM_RING_FULL_DROPS);
    PRINT_COUNTER(CNTR_DEBUG_CNT_FRAGMENT_DEST_INVALID);
    PRINT_COUNTER(CNTR_DEBUG_CNT_LB_NO_VALID_DEST_IDS);
    PRINT_COUNTER(CNTR_DEBUG_CNT_IP_FRAGMENT_IDENTIFICATION_ERROR);
    PRINT_COUNTER(CNTR_DEBUG_CNT_FRAGMENT_ID_REQUEST_FAIL);
    PRINT_COUNTER(CNTR_DEBUG_CNT_802_3_NON_IP);
    PRINT_COUNTER(CNTR_DEBUG_CNT_IP_HDR_ERR);
    PRINT_COUNTER(CNTR_DEBUG_CNT_TCP_UDP_HDR_ERR);
    PRINT_COUNTER(CNTR_DEBUG_CNT_PKT_TO_IA_CR_ALLOC_FAIL);
    PRINT_COUNTER(CNTR_DEBUG_CNT_PKT_TO_IA_BUF_ALLOC_FAIL);
    PRINT_COUNTER(CNTR_DEBUG_CNT_SAME_KEY_ZERO_RESULT_NO_DROP);
    PRINT_COUNTER(CNTR_DEBUG_CNT_SAME_KEY_ZERO_RESULT_DROP);

    //using the ns_cntr_send()/ns_cntr_recv() pairs read a single count
    index = CNTR_DEBUG_CNT_802_3_NON_IP;
    ret = ns_cntr_send(msg_h, index, &seq_no);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
        NS_LOG_ERROR("ns_cntr_send(%u,%u) (%d,%d): %s",
                     index,
                     seq_no,
                     NS_NFM_ERROR_CODE(ret),
                     NS_NFM_ERROR_SUBCODE(ret),
                     ns_nfm_error_string(ret));
        return 1;
    }
    while(1) {
        ret = ns_cntr_recv(msg_h, seq_no, &raw);
        if (NS_NFM_ERROR_CODE(ret) == NS_NFM_SUCCESS) {
            FORMAT_COUNTER(CNTR_DEBUG_CNT_802_3_NON_IP);
            break;
        }
        else if (NS_NFM_ERROR_CODE(ret) == NS_NFM_RETRY_LATER) {
            //do something else for a bit if you like
            usleep(2000);
        } else {
            NS_LOG_ERROR("ns_cntr_recv(%u) (%d,%d): %s",
                         seq_no,
                         NS_NFM_ERROR_CODE(ret),
                         NS_NFM_ERROR_SUBCODE(ret),
                         ns_nfm_error_string(ret));
            return 1;
        }
    }


    /* using the ns_cntr_send()/ns_cntr_recv() pairs to poll a single
     * counter, maximizing no-blocking
     * start by priming the pump, do this once */
    index = CNTR_DEBUG_CNT_802_3_NON_IP;
    ret = ns_cntr_send(msg_h, index, &seq_no);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
        NS_LOG_ERROR("ns_cntr_send(%u,%u) (%d,%d): %s", index, seq_no,
                     NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret),
                     ns_nfm_error_string(ret));
        return 1;
    }
    //- your main loop that will do the polling
    i = 0;
    while(1) {
        //read the prior request
        ret = ns_cntr_recv(msg_h, seq_no, &raw);
        if (NS_NFM_ERROR_CODE(ret) == NS_NFM_SUCCESS) {
            FORMAT_COUNTER(CNTR_DEBUG_CNT_802_3_NON_IP);
            //start the next request
            ret = ns_cntr_send(msg_h, index, &seq_no);
            if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS) {
                NS_LOG_ERROR("ns_cntr_send(%u,%u) (%d,%d): %s", index, seq_no,
                             NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret),
                             ns_nfm_error_string(ret));
                return 1;
            }
            i++;
            if (i >= 10)
                break;
        } else if (NS_NFM_ERROR_CODE(ret) == NS_NFM_RETRY_LATER) {
            //data is not ready yet
        } else {
            NS_LOG_ERROR("ns_cntr_recv(%u) (%d,%d): %s", seq_no,
                         NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret),
                         ns_nfm_error_string(ret));
            return 1;
        }

        //do whatever you like here
        usleep(100000);
    }
    //- when finished clean up the last request
    while(1) {
        ret = ns_cntr_recv(msg_h, seq_no, &raw);
        if (NS_NFM_ERROR_CODE(ret) == NS_NFM_SUCCESS) {
            FORMAT_COUNTER(CNTR_DEBUG_CNT_802_3_NON_IP);
            break;
        } else if (NS_NFM_ERROR_CODE(ret) == NS_NFM_RETRY_LATER)  {
            //do something else for a bit if you like
            usleep(2000);
        } else {
            NS_LOG_ERROR("ns_cntr_recv(%u) (%d,%d): %s", seq_no,
                         NS_NFM_ERROR_CODE(ret),
                         NS_NFM_ERROR_SUBCODE(ret),
                         ns_nfm_error_string(ret));
        }
    }

    for (i = 0; i < (COUNTERS_PER_PORT * MAX_NFE_PORTS); ++i) {
        if (wrap_ns_cntr(msg_h, CNTR_DEBUG_PORT_0_PACKET_PKTS_RECEIVED + i,
                         &raw))
          return 1;
        printf("%-50s%12lu\n", RxPortStats[i], (unsigned long)raw);
    }


    for (i = 0; i < CNTRTAB_NUM_ROWS; i++) {
        char str[51];
        ret = ns_cntr_region(msg_h, NS_CNTR_READ, CNTRTAB_APP_SENT,
                             i, i, 0, -1, &raw);
        sprintf(str, "Row %d of CNTRTAB_APP_SENT", i);
        FORMAT_COUNTER_S(str);
    }

    for (i = 0; i < CNTRTAB_NUM_COLS; i++) {
        char str[51];
        ret = ns_cntr_region(msg_h, NS_CNTR_READ, CNTRTAB_APP_SENT,
                             0, -1, i, i, &raw);
        sprintf(str, "Column %d of CNTRTAB_APP_SENT", i);
        FORMAT_COUNTER_S(str);
    }

    ret = ns_cntr_region(msg_h, NS_CNTR_READ, CNTRTAB_APP_SENT, 0, -1, 0, -1,
                         &raw);
    FORMAT_COUNTER_S("All of CNTRTAB_APP_SENT");

    for (i = 0; i < CNTRTAB_NUM_ROWS; i++)
        cntrs[i] = i;

    /* get the raw data for each row */
    for (i = 0; i < CNTRTAB_NUM_ROWS; i++) {
        ret = ns_cntr_row_send_ex(msg_h, CNTRTAB_APP_UNAV, i, &seq_no);
        j = 0;
        while(1) {
            num_cntrs = CNTRTAB_NUM_ROWS;
            ret = ns_cntr_recv_ex(msg_h, seq_no, &raw, &num_cntrs, cntrs);
            if (NS_NFM_ERROR_CODE(ret) == NS_NFM_SUCCESS)
                break;
            if (NS_NFM_ERROR_CODE(ret) != NS_NFM_RETRY_LATER) {
                NS_LOG_ERROR("ns_cntr_recv_ex(%u) (%d,%d): %s", seq_no,
                             NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret),
                             ns_nfm_error_string(ret));
                return 1;
            }

            j++;
            if (j >= 10) {
                NS_LOG_ERROR("ns_cntr_recv_ex(%u) timed out.", seq_no);
                return 1;
            }
            usleep(100000);
        }
        printf("Raw CNTRTAB_APP_UNAV for row %d (%d counters): Sum = %lu\n",
               i, num_cntrs, raw);
        for (j = 0; j < num_cntrs; j++)
            printf("  Row %d Column %2d: %12lu\n", i, j, cntrs[j]);
    }

    ret = ns_cntr_region(msg_h, NS_CNTR_READ, CNTRTAB_APP_UNAV, 0, -1, 0, -1,
                         &raw);
    FORMAT_COUNTER_S("All of CNTRTAB_APP_UNAV");

    /* get the raw data for each column */
    for (i = 0; i < CNTRTAB_NUM_COLS; i++) {
        ret = ns_cntr_col_send_ex(msg_h, CNTRTAB_APP_UNAV, i, &seq_no);
        j = 0;
        while(1) {
            num_cntrs = CNTRTAB_NUM_ROWS;
            ret = ns_cntr_recv_ex(msg_h, seq_no, &raw, &num_cntrs, cntrs);
            if (NS_NFM_ERROR_CODE(ret) == NS_NFM_SUCCESS)
                break;
            if (NS_NFM_ERROR_CODE(ret) != NS_NFM_RETRY_LATER) {
                NS_LOG_ERROR("ns_cntr_recv_ex(%u) (%d,%d): %s", seq_no,
                             NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret),
                             ns_nfm_error_string(ret));
                return 1;
            }

            j++;
            if (j >= 10) {
                NS_LOG_ERROR("ns_cntr_recv_ex(%u) timed out.", seq_no);
                return 1;
            }
            usleep(100000);
        }
        printf("Raw CNTRTAB_APP_UNAV for column %d (%d counters): Sum = %lu\n",
               i, num_cntrs, raw);
        for (j = 0; j < num_cntrs; j++)
            printf("  Columns %2d Row %2d: %12lu\n", i, j, cntrs[j]);
    }


    ret = ns_cntr_region(msg_h, NS_CNTR_READ, CNTRTAB_APP_UNAV, 0, -1, 0, -1,
                         &raw);
    FORMAT_COUNTER_S("All of CNTRTAB_APP_UNAV");

    printf("Done.\n");
    return 0;
}
