/**
 * Copyright (C) 2008-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_linkstate.c
 *
 * Description: This is an application demonstrating the usage of the
 *              various ns_link_state.h and nfe_interface.h APIs.  This
 *              application will take as input arbitrary port sets to
 *              mirror link state among.  Functionality demonstrated in
 *              this application are linkstate callback events,
 *              activating/deactivating mirroring, and reading link status
 *              synchronously.
 */

#include "ns_link_state.h"
#include "nfe_interface.h"
#include "nfm_platform.h"
#include "ns_log.h"
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>

static int g_running = 1, mirroring_enabled = 0, h_up=5000, h_down=3000;
static ns_linkstate_h mirror_h, cb_h;

void quitme(int signum)
{
  (void)signum;
  g_running = 0;
}

//Callback for the linkstate events
void __linkstate_cb(ns_linkstate_h ls_h __attribute__((unused)), ns_linkstate_event_e event, ns_portset_t p_trigger, ns_link_status_t S, void* cb_ctx __attribute__((unused)))
{
  ns_nfm_ret_t ret;
  unsigned i;
  switch(event)
  {
    case LSE_INITIAL_STATUS:
      for (i = 1; i <= S.nports; i++) {
        if ((S.ports[i].valid != 0) && (S.ports[i].los != 0)) {
          NS_LOG_INFO("Initial status: link failure on %u", i);
          if (!mirroring_enabled) {
            NS_LOG_INFO("Activating mirroring");
            if((ret=ns_linkstate_activate_mirror(mirror_h, 0))!=NS_NFM_SUCCESS) {
              NS_LOG_ERROR("Error in ns_linkstate_activate_mirror: %s\n", ns_nfm_error_string(ret));
            }
            mirroring_enabled=1;
          }
        }
      }
      break;
    case LSE_LINK_FAILURE:
      NS_LOG_INFO("LSE_LINK_FAILURE, %x", (unsigned int)p_trigger);
      break;
    case LSE_ALL_LINKS_UP:
      NS_LOG_INFO("LSE_ALL_LINKS_UP, %x", (unsigned int)p_trigger);
      NS_LOG_INFO("Deactivating mirroring");
      if((ret=ns_linkstate_deactivate_mirror(mirror_h))!=NS_NFM_SUCCESS) {
        NS_LOG_ERROR("Error in ns_linkstate_deactivate_mirror: %s\n", ns_nfm_error_string(ret));
      }
      mirroring_enabled=0;
      break;
    case LSE_PORT_GROUP_DOWN:
      NS_LOG_INFO("LSE_PORT_GROUP_DOWN, %x", (unsigned int)p_trigger);
      if(!mirroring_enabled) {
        NS_LOG_INFO("Activating mirroring");
        if((ret=ns_linkstate_activate_mirror(mirror_h, 0))!=NS_NFM_SUCCESS) {
          NS_LOG_ERROR("Error in ns_linkstate_activate_mirror: %s\n", ns_nfm_error_string(ret));
        }
        mirroring_enabled=1;
      }
      break;
    case LSE_PORT_GROUP_UP:
      NS_LOG_INFO("LSE_PORT_GROUP_UP, %x", (unsigned int)p_trigger);
      break;
    case LSE_STATUS_UPDATE:
      NS_LOG_DEBUG("LSE_STATUS_UPDATE, %x", (unsigned int)p_trigger);
      break;
  }
}

//Interface status callback
void __nfe_interface_cb(nfe_interface_h h __attribute__((unused)), interface_port_bitmap_t p, const nfe_interface_status_t nfe_stat, void* cb_ctx __attribute__((unused)))
{
  NS_LOG_DEBUG("Link status update callback");
  NS_LOG_INFO("Ports changed     : %s", nfe_interface_port_bitmap_to_string(p));
  NS_LOG_INFO("Ports with LoS    : %s", nfe_interface_LoS_to_string(nfe_stat));
  NS_LOG_INFO("Ports forced down : %s", nfe_interface_Disabled_to_string(nfe_stat));
}

//Print out the ports which are currently down
void log_down(ns_portset_t down, unsigned int nports)
{
  char str[NFM_MAX_PORTS*20+3] = "[", numstr[20];
  unsigned int i, first = 1;

  for (i = 1; i <= nports; i++) {
    if (NS_PSET_TEST(down, i)) {
      if (first)
        first = 0;
      else
        strcat(str, ",");
      sprintf(numstr, "%d", i);
      strcat(str, numstr);
    }
  }
  strcat(str, "]");
  NS_LOG_INFO("Ports currently down (polling): %s", strlen(str)>2?str:"None");
}

void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
  "  where options are: \n"
  "  -s return link status and exit\n"
  "  -h this message \n"
  "  -l log level\n"
  "  -L enable latching (i.e. if ports get mirrored down, keep down until manual intervention)\n"
  "  -H [down_ms,up_ms] hysteresis values for mirroring (DEFAULT: 5000ms up, 3000ms down)\n"
  "  -m [bitmask1,bitmask2,...,bitmask6] group of portsets defined by bitmasks\n"
  "     Used in mirroring and for callbacks.  From 1 to 6 portsets can be supplied.\n"
  "     i.e. 0xC00,0x300,0x0C0,0x030,0x00C,0x003 if you want to mirror adjacent port pairs.\n"
  "          or 0xfff to mirror all\n\n"
  "This application demonstrates the functionality of various NFM linkstate APIs.\n"
  "If executed with the -s option, it will simply return the link status.  Otherwise it will\n"
  "monitor links and enable mirroring for the port sets given by the -m option.  Monitoring\n"
  "is done both by printing out the linkstate every 10 seconds and by printing information upon\n"
  "linkstate events (link down/up etc.) via a callback function.\n"
  "The state of a port can be mirrored to any number of ports.  This is by represented one or\n"
  "more bitmasks which are provided to the -m option.\n"
  , argv0);
  exit(1);
}

static void log_callback (ns_log_lvl_e lvl, const char* file __attribute__ ((unused)), unsigned int line __attribute__ ((unused)), const char* func __attribute__ ((unused)), const char* str) {
    int sysloglevel;

    switch ( lvl ) {
        case NS_LOG_LVL_NONE:
            sysloglevel = -1;
            break;
        case NS_LOG_LVL_FATAL:
            sysloglevel = LOG_EMERG;
            break;
        case NS_LOG_LVL_ERROR:
            sysloglevel = LOG_ERR;
            break;
        case NS_LOG_LVL_WARN:
            sysloglevel = LOG_WARNING;
            break;
        case NS_LOG_LVL_INFO:
            sysloglevel = LOG_INFO;
            break;
        case NS_LOG_LVL_DEBUG:
            sysloglevel = LOG_DEBUG;
            break;
        case NS_LOG_LVL_EXTRA:
            sysloglevel = LOG_NOTICE;
            break;
        case NS_LOG_LVL_HEAVY:
            sysloglevel = LOG_NOTICE;
            break;
        case NS_LOG_LVL_VERBOSE:
            sysloglevel = LOG_NOTICE;
            break;
        default:
            sysloglevel = -1;
            break;
    }
    if ( sysloglevel != -1 )
        syslog(sysloglevel, "%s", str);
}

static void configure_logging (ns_log_lvl_e loglevel) {

    /* Argument check */
    if ( loglevel > 9 ) {
        printf("Please enter a loglevel from 0 to 9\n");
        exit(EXIT_FAILURE);
    }

    /* Initialise logging */
    ns_log_init(NS_LOG_NO_FLAGS);
    openlog("l2_util", LOG_ODELAY | LOG_PID, LOG_USER);
    setlogmask(LOG_UPTO(LOG_DEBUG));
    ns_log_set_cb(log_callback);

    /* Configure NFM log level */
    if ( loglevel < 9 ) {
        ns_log_lvl_set(loglevel);
    } else { //log level = 9
        ns_log_init(NS_LOG_COLOR | NS_LOG_CONSOLE);
        ns_log_lvl_set(8);
    }
}

int main(int argc, char *argv[])
{
  ns_nfm_ret_t ret;
  ns_linkstate_h mon_h;
  nfe_interface_h interface_h;
  nfe_interface_h nfe_h[MAX_NFES];
  ns_link_status_t stat;
  unsigned int i, nports, nnfes, latching=0, get_stats=0, num_portsets=0;
  ns_portset_t ps;
  unsigned int portsets[6];
  static ns_log_lvl_e loglevel = NS_LOG_LVL_INFO;
  nfe_interface_status_t nfe_stat;
  char status_str[NFM_MAX_PORTS*3+2], force_str[NFM_MAX_PORTS*3+2];
  static struct option long_options[] = {
    {"hysteresis",          1,0,'H'},
    {"help",                0,0,'h'},
    {"status",              0,0,'s'},
    {"monitor",             1,0,'M'},
    {"loglevel",            1,0,'l'},
    {0,0,0,0}
  };
  int r;

  //get options
  while ((r = getopt_long(argc, argv, "hsH:m:Ll:", long_options, NULL)) != -1) {
    switch (r) {
      case 'l':
        loglevel = (unsigned int)strtoul(optarg,0,0);
        break;
      case 'H':
        if(sscanf(optarg, "%d,%d", &h_down, &h_up)!=2) {
          print_usage(argv[0]);
        }
        break;
      case 'L':
        latching=1;
        break;
      case 'h':
        print_usage(argv[0]);
        break;
      case 's':
        get_stats=1;
        break;
      case 'm':
        printf("%s\n", optarg);
        num_portsets=sscanf(optarg, "%i,%i,%i,%i,%i,%i",
                            &portsets[0],&portsets[1],&portsets[2],&portsets[3],&portsets[4],&portsets[5]);
        break;
    }
  }

  if (optind != argc)
    print_usage(argv[0]);

  signal(SIGINT, quitme);

  //log initialization
  configure_logging(loglevel);
  NS_LOG_INFO("Starting with loglevel %d", loglevel);

  if((ret=ns_linkstate_open(&mon_h))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in ns_linkstate_open:  %s\n", ns_nfm_error_string(ret));
  }
  //The linkset handle mon_h will only be used for monitoring ports synchronously
  //and will include all available ports
  nports = ns_linkstate_get_num_ports(mon_h);
  ps = NS_PSET_FULL_SET(nports);
  if((ret=ns_linkstate_add_port_group(mon_h, ps))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in ns_linkstate_add_port_group:  %s\n", ns_nfm_error_string(ret));
  }
  //Get the number of NFEs installed in the system
  if((ret=nfm_get_nfes_present(&nnfes))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in nfm_get_nfes_present: %s\n", ns_nfm_error_string(ret));
  }
  //Print out link state and exit
  if(get_stats)
  {
    strcpy(force_str, "");
    strcpy(status_str, "");
    printf("Status of Interface:\n");
    for(i = 0; i < nnfes; i++) {
      if((ret=nfe_interface_init(i, &interface_h))!=NS_NFM_SUCCESS) {
        NS_LOG_ERROR("Error in nfe_interface_init: %s\n", ns_nfm_error_string(ret));
      }
      if((ret=nfe_interface_get_status(interface_h, NFE_INTERFACE_ALL_PORTS, &nfe_stat))!=NS_NFM_SUCCESS) {
        NS_LOG_ERROR("Error in nfe_interface_get_status: %s\n", ns_nfm_error_string(ret));
      }
      printf("Ports on NFE %d    : %s\n", i, nfe_interface_port_bitmap_to_string(NFE_INTERFACE_ALL_PORTS));
      printf("Ports with LoS    : %s\n", nfe_interface_LoS_to_string(nfe_stat));
      printf("Ports forced state : %s\n", nfe_interface_Disabled_to_string(nfe_stat));
      nfe_interface_close(interface_h);
    }
    printf("Status of NFE ports:\n");
    if((ret=ns_linkstate_get_status(mon_h, &stat))!=NS_NFM_SUCCESS) {
      NS_LOG_ERROR("Error in ns_linkstate_get_status: %s\n", ns_nfm_error_string(ret));
    }
    printf("link    ");
    for (i = 1; i <= nports; i++) {
      printf("%3d", i);
      if(stat.ports[i].los) {
        strcat(status_str, "  D");
      }
      else {
        strcat(status_str, "  U");
      }
      if(stat.ports[i].forced) {
        strcat(force_str, "  F");
      }
      else {
        strcat(force_str, "  A");
      }
    }
    printf("\nstate   %s\n", status_str);
    printf("forced  %s\n", force_str);
    ns_linkstate_close(mon_h);
    exit(0);
  }

  //Now we will set up linkstate mirroring on the ports and register for link
  //monitoring callbacks

  //Must specify a bitmask of ports to monitor
  if(num_portsets<1) {
    ns_linkstate_close(mon_h);
    print_usage(argv[0]);
  }

  //Open two linkstate handles, one for mirroring and one for receiving callbacks
  if((ret=ns_linkstate_open(&mirror_h))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in ns_linkstate_open: %s\n", ns_nfm_error_string(ret));
  }
  if((ret=ns_linkstate_open(&cb_h))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in ns_linkstate_open: %s\n", ns_nfm_error_string(ret));
  }

  //Add all the specified port sets (a port group consists of one or more port
  //sets) to both handles
  for (i = 0; i < num_portsets; i++) {
    NS_LOG_DEBUG("Adding port group %x\n", portsets[i]);
    if((ret=ns_linkstate_add_port_group(mirror_h, portsets[i]))!=NS_NFM_SUCCESS) {
      NS_LOG_ERROR("Error in ns_linkstate_add_port_group: %s\n", ns_nfm_error_string(ret));
    }
    if((ret=ns_linkstate_add_port_group(cb_h, portsets[i]))!=NS_NFM_SUCCESS) {
      NS_LOG_ERROR("Error in ns_linkstate_add_port_group: %s\n", ns_nfm_error_string(ret));
    }
  }

  //If latching is enabled, then ports will mirror and then stay down until
  //manually unforced
  if(latching) {
    NS_LOG_INFO("setting latching\n");
    if((ret=ns_linkstate_set_latching(mirror_h))!=NS_NFM_SUCCESS) {
      NS_LOG_ERROR("Error in ns_linkstate_set_latching: %s\n", ns_nfm_error_string(ret));
    }
  }

  //We will activate/deactivate mirroring in the link state callback
  //You could do it here instead if desired
  //ns_linkstate_activate_mirror(mirror_h, hyst);

  //Enable hysteresis for callback events.  For example, if a port gets a loss of signal
  //the duration of the loss of signal needs to be > the debounce down time for us to get
  //a callback event
  NS_LOG_INFO("setting hysteresis to: %d, %d\n", h_down, h_up);
  if((ret=ns_linkstate_set_hysteresis(cb_h, h_down, h_up))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in ns_linkstate_set_hysteresis: %s\n", ns_nfm_error_string(ret));
  }
  if((ret=ns_linkstate_set_hysteresis(mirror_h, h_down, h_up))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in ns_linkstate_set_hysteresis: %s\n", ns_nfm_error_string(ret));
  }
  //Now register for the linkstate callback
  if((ret=ns_linkstate_register_cb(cb_h, __linkstate_cb, NULL))!=NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Error in ns_linkstate_register_cb: %s\n", ns_nfm_error_string(ret));
  }

  //Register for nfe_interface callback updates
  NS_LOG_INFO("Registering for nfe_interface status callbacks");
  for(i = 0; i < nnfes; i++) {
    //Open handle for each card
    if((ret=nfe_interface_init(i, &nfe_h[i]))!=NS_NFM_SUCCESS) {
      NS_LOG_ERROR("Error in nfe_interface_init: %s\n", ns_nfm_error_string(ret));
    }
    //Now register for the interface callback
    if((ret=nfe_interface_register_for_status_updates(nfe_h[i], __nfe_interface_cb, NULL))!=NS_NFM_SUCCESS) {
      NS_LOG_ERROR("Error in nfe_interface_register_for_status_updates: %s\n", ns_nfm_error_string(ret));
    }
  }

  //Poll the link status in addition to the callbacks
  while (g_running) {
    if((ret=ns_linkstate_get_status(mon_h, &stat))!=NS_NFM_SUCCESS) {
      NS_LOG_ERROR("Error in ns_linkstate_get_status: %s\n", ns_nfm_error_string(ret));
    }
    ps = NS_PSET_EMPTY_SET;
    for (i = 1; i <= nports; i++) {
      if ((stat.ports[i].los|stat.ports[i].forced) != 0)
        NS_PSET_ADD(ps, i);
    }
    log_down(ps, nports);
    sleep(10);
  }

  NS_LOG_INFO("Closing active handles");
  for(i=0; i<nnfes; i++) {
    nfe_interface_close(nfe_h[i]);
  }
  ns_linkstate_close(mon_h);
  ns_linkstate_close(cb_h);
  ns_linkstate_close(mirror_h);

  return 0;
}
