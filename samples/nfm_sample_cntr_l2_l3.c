
//-------------------------------------------------------------------------
// Copyright (C) 2011 Netronome Systems, Inc.  All rights reserved.
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

#define _PRINT_COUNTER_ITEM(item, c, value64) printf("%4u %-50s%18lu\n", item, #c, value64)

#define PRINT_COUNTER_GLOBAL(c, value64)       printf("GLOBAL COUNTER   :               %-50s%18lu\n", #c, value64)
#define PRINT_COUNTER_LIF(num, c, value64)     printf("LOG INTF COUNTER : Log intf "); _PRINT_COUNTER_ITEM(num, c, value64)
#define PRINT_COUNTER_PIF(num, c, value64)     printf("PHY INTF COUNTER : Phy intf "); _PRINT_COUNTER_ITEM(num, c, value64)
#define PRINT_COUNTER_VIF(num, c, value64)     printf("VIR INTF COUNTER : Vir intf "); _PRINT_COUNTER_ITEM(num, c, value64)
#define PRINT_COUNTER_VBRIDGE(num, c, value64) printf("VBRIDGE COUNTER  : Vbridge  "); _PRINT_COUNTER_ITEM(num, c, value64)
#define PRINT_COUNTER_VROUTER(num, c, value64) printf("VROUTER COUNTER  : Vrouter  "); _PRINT_COUNTER_ITEM(num, c, value64)


//-------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    ns_nfm_ret_t ret;
    uint64_t counter_val;
    unsigned int lif_num;
    unsigned int pif_num;
    unsigned int vbridge_id;
    unsigned int vif_num;
    unsigned int vrouter_id;

    if(argc != 1) {
        printf("usage: %s\n", argv[0]);
        return 1;
    }

    //See "ns_cntr_global.h" for a full list of Global counters
#if 0
   // Currently no global counters have been implemented
    ret = ns_cntr_read_global(NS_CNTR_GLOBAL_FOO , &counter_val);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS){
        printf("Error reading Global counter NS_CNTR_GLOBAL_FOO !\n");
    }
    else{
        PRINT_COUNTER_GLOBAL(NS_CNTR_GLOBAL_DROP_MODE, counter_val);
    }
#endif

    //See "ns_cntr_lif.h" for a full list of per Logical Interface counters
    lif_num = 10;    //0-1023
    ret = ns_cntr_read_lif(lif_num, NS_CNTR_LIF_PKTS_TRANSMITTED_UNICAST, &counter_val);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS){
        printf("Error reading Logical Interface %u counter NS_CNTR_LIF_PKTS_TRANSMITTED_UNICAST !\n", lif_num);
    }
    else{
        PRINT_COUNTER_LIF(lif_num, NS_CNTR_LIF_PKTS_TRANSMITTED_UNICAST, counter_val);
    }

    //See "ns_cntr_pif.h" for a full list of per Physical Interface counters
    pif_num = 13;    //0-63
    ret = ns_cntr_read_pif(pif_num, NS_CNTR_PIF_PKTS_TRANSMITTED, &counter_val);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS){
        printf("Error reading Physical Interface %u counter NS_CNTR_PIF_PKTS_TRANSMITTED !\n", pif_num);
    }
    else{
        PRINT_COUNTER_PIF(pif_num, NS_CNTR_PIF_PKTS_TRANSMITTED, counter_val);
    }


    //See "ns_cntr_vbridge.h" for a full list of per Virtual Bridge counters
    vbridge_id = 200; //0-511
    ret = ns_cntr_read_vbridge(vbridge_id, NS_CNTR_VBRIDGE_BROADCAST_PKTS_RECEIVED, &counter_val);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS){
        printf("Error reading Virtual Bridge %u counter NS_CNTR_VBRIDGE_BROADCAST_PKTS_RECEIVED !\n", vbridge_id);
    }
    else{
        PRINT_COUNTER_VBRIDGE(vbridge_id, NS_CNTR_VBRIDGE_BROADCAST_PKTS_RECEIVED, counter_val);
    }

    //See "ns_cntr_vif.h" for a full list of per Virtual Interface counters
    vif_num = 3; //0-1023
    ret = ns_cntr_read_vif(vif_num, NS_CNTR_VIF_ARP_REQUESTS_RECEIVED, &counter_val);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS){
        printf("Error reading Virtual Interface %u counter NS_CNTR_VIF_ARP_REQUESTS_RECEIVED !\n", vif_num);
    }
    else{
        PRINT_COUNTER_VIF(vif_num, NS_CNTR_VIF_ARP_REQUESTS_RECEIVED, counter_val);
    }

    //See "ns_cntr_vrouter.h" for a full list of per Virtual Router counters
    vrouter_id = 20; //0-511
    ret = ns_cntr_read_vrouter(vrouter_id, NS_CNTR_VROUTER_UNICAST_PKTS_ROUTED_NOWHERE, &counter_val);
    if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS){
        printf("Error reading Virtual Router %u counter NS_CNTR_VROUTER_UNICAST_PKTS_ROUTED_NOWHERE !\n", vrouter_id);
    }
    else{
        PRINT_COUNTER_VROUTER(vrouter_id, NS_CNTR_VROUTER_UNICAST_PKTS_ROUTED_NOWHERE, counter_val);
    }


    printf("Done.\n");
    return 0;
}
