#include <stdio.h>
#include "nfm_platform.h"

int main() {

  nfm_microengine_monitor_start();

  while (1) {
    nfm_microengine_monitor_t mon;

    if (nfm_microengine_monitor(&mon) == NS_NFM_SUCCESS) {
        printf("card: %d reason: %u, reason_param 0x%x, msg: '%s'\n",
               mon.nfe_card_num, mon.reason, mon.reason_param, mon.msg);
        break;
    }
  }

  nfm_microengine_monitor_stop();
  return 0;
}
