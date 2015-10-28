/**
 * Copyright (C) 2004-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_monitor.c
 * Description: Sample monitor daemon for NFM. This will need
 *              customization for a specific application to be
 *              useful. Right now it just logs any failures it detects
 *              in syslog.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <syslog.h>
#include <memory.h>
#include <libgen.h>

#include "nfm_platform.h"
#include "nfe.h"

#ifdef NS_PLATFORM
#include "scmd.h"
#include "psls.h"
#include "ftwo.h"
#ifdef GERCHR
#include "gerd.h"
#include "gpsl.h"
#include "gftw.h"
#endif /* GERCHR */
#endif

#define LOG_DEST LOG_LOCAL1
#define LOGNAME_SIZE 10

#define MAXIMUM_ALLOWED_NFE_TEMP 100
#define NFD_POOL_HIGHWATER 90

char daemon_name[32];
char pidfile[60];

int notdone = 1;
int debug = 0;
int no_daemonize = 0;

#ifndef NS_MSG_BUILD_ERROR_CODE
#define NS_MSG_BUILD_ERROR_CODE(c,s) (((c)&0x000000ff)<<8 | ((s)&0x000000ff))
#endif

void exitfunc()
{
  unlink(pidfile);
}

static void sigint_handler(int s __attribute__ ((unused)))
{
  static int terminated = 0;
  if (!terminated) {
    notdone = 0;
    terminated = 1;
  }
}

void usage(void)
{
  printf("NFM sample monitor\n");
  printf("\n");
  printf("Sample monitor daemon for NFM. This will need\n");
  printf("customization for a specific application to be\n");
  printf("useful. Right now it just logs any failures it detects\n");
  printf("in syslog.\n");
  printf("\n");
  printf("Usage is:\n");
  printf("  -d         Debug mode. Do not daemonize.\n");
  printf("  -n         Do not daemonize.\n");
  printf("  -h         Print this usage help.\n");
}

/* use ps and grep to see if a daemon is running on the host. */
static int check_daemon(const char *name, int device)
{
  int rc = 0;
  char cmd[256];
  char tempfile[128] = "/tmp/nfm_sample_monitor.tmp.XXXXXX";
  int file;

  file = mkstemp(tempfile);
  close(file);
  snprintf(cmd, sizeof(cmd), "PATH=$PATH:/sbin ps ax | grep %s > %s 2>&1",
           name, tempfile);
  system(cmd);

  snprintf(cmd, sizeof(cmd),
           "PATH=$PATH:/sbin grep \"\\-d *%d\" %s > /dev/null 2>&1", device,
           tempfile);
  if (system(cmd) == 0) {
    rc = 0;
  } else {
    rc = 1;
  }

  remove(tempfile);

  return rc;
}

/* use nfm utility to see if exception deamon is running */
static int check_exceptiond()
{
  int rc = 0;
  char cmd[256];

  snprintf(cmd, sizeof(cmd), "PATH=$PATH:/opt/netronome/bin"
          " nfm exceptiond show > /dev/null 2>&1");

  if (system(cmd) == 0) {
    rc = 0;
  } else {
    rc = 1;
  }

  return rc;
}

/* See if we can ping the IP stack on the NFE. */
static int check_NFE_ip(int device)
{
  char cmd[80];
  struct in_addr ip_addr;
  nfm_get_nfe_ip_addr(device, &ip_addr);
  sprintf(cmd, "ping -c 1 %s > /dev/null 2>&1", inet_ntoa(ip_addr));
  return (system(cmd));
}

/* See what the measured tempeture is on the NFE. */
static int check_NFE_temp(int device)
{
  nfe_ret_t ret;
  struct nfe_state_t nfe_state;

  if (NFE_ERROR_CODE(ret = nfe_device_state(device, &nfe_state)) !=
      NFE_SUCCESS) {
    syslog(LOG_DEST | LOG_INFO, "nfe_device_state(%u) failed: %s\n",
           device, nfe_error_string(ret));
    return 1;
  }
  if (nfe_state.npu_temp > MAXIMUM_ALLOWED_NFE_TEMP) {
    syslog(LOG_DEST | LOG_INFO,
           "NFE temperature over limit on NFE %d. Currently %d and limit is %d\n",
           device, nfe_state.npu_temp, MAXIMUM_ALLOWED_NFE_TEMP);
    return 1;
  }
  return 0;
}

/* See if we can open the platform daemon on the NFE and check the me heartbeats. */
static int check_platform_daemon_and_me(int device, uint32_t * heartbeats)
{
  struct nfm_pd_desc pd;
  int rc;

  rc = nfm_pd_open(&pd, device);
  if (rc != 0)
    return 1;
  rc = nfm_pd_read_me_heartbeats(&pd, heartbeats);
  nfm_pd_close(&pd);
  if (rc == 0) {
    return 0;
  } else {
    return 1;
  }
}

/* See if we can open the platform daemon on the NFE and check the me heartbeats. */
static int check_platform_endpoints(int device)
{
  struct nfm_pd_desc pd;
  int rc;

  rc = nfm_pd_open(&pd, device);
  if (rc != 0)
    return -1;
  rc = nfm_pd_read_endpoint_shutdowns(&pd);
  nfm_pd_close(&pd);
  return rc;
}

#ifdef NS_PLATFORM
/* Check the NMSB out. */
#define SWITCH_CMD_LEN 256
static int check_nmsb_daemons(int *swicthd, int *attached_card, int *portd,
                              int *failoverd)
{
  int rc, nmsb_up;
  unsigned int devices;
  struct sc_desc sd;
  struct pl_desc pd;
  struct fo_desc fd;
  char cmd[SWITCH_CMD_LEN], ip6_str[SWITCH_CMD_LEN];
  struct in_addr ip_addr;
  struct in6_addr ip6_addr;
  ns_nfm_ret_t ret;

  /* Initialise descriptors */
  memset(&sd, 0, sizeof (sd));
  memset(&pd, 0, sizeof (pd));
  memset(&fd, 0, sizeof (fd));

  /* really quick check to see if we even have an NMSB */
  devices = nmsb_num_of_devices();
  if (devices != 1) {
    *swicthd = 1;
    *attached_card = 1;
    *portd = 1;
    *failoverd = 1;
    return (2);
  }

  nmsb_up = 0;
  *swicthd = 0;
  *attached_card = 0;
  *portd = 0;
  *failoverd = 0;

  /* open the swicthd daemon */
  rc = scmd_open(&sd, SC_AP_ID_USER1);
  if (rc != 0) {
    *swicthd = 1;
    nmsb_up = 1;
  } else {
    rc = scmd_close(&sd);
  }

  /* open the portd daemon */
  rc = psls_open(&pd, PL_AP_ID_USER1);
  if (rc != 0) {
    *portd = 1;
    nmsb_up = 1;
  } else {
    rc = psls_close(&pd);
  }

  /* open the failoverd daemon */
  rc = ftwo_open(&fd, FO_AP_ID_USER1);
  if (rc != 0) {
    *failoverd = 1;
    nmsb_up = 1;
  } else {
    rc = ftwo_close(&fd);
  }

  /* if we can't talk to all three daemons check the interface. */
  ret = nfm_get_nmsb_ip_addr(0, &ip_addr);
  if (ret == NS_NFM_SUCCESS)
      snprintf(cmd, SWITCH_CMD_LEN,
               "ping -c 1 %s > /dev/null 2>&1", inet_ntoa(ip_addr));
  else {
      /* we may use IPv6 addresses */
      ret = nfm_get_nmsb_ipv6_addr(0, &ip6_addr);
      if (ret != NS_NFM_SUCCESS)
          return nmsb_up;

      inet_ntop(AF_INET6, &ip6_addr, ip6_str, SWITCH_CMD_LEN);
      snprintf(cmd, SWITCH_CMD_LEN, "ping6 -c 1 %s > /dev/null 2>&1", ip6_str);
  }

  if ((*swicthd != 0) && (*portd != 0) && (*failoverd != 0)) {
      rc = system(cmd);
      if (rc != 0) {
          nmsb_up = 3;
      }
  }

  return (nmsb_up);
}

#ifdef GERCHR
static int check_gerchr_daemons(int *swicthd, int *attached_card, int *portd,
                              int *failoverd) {
  int rc, gerchr_up;
  unsigned int devices;
  struct ge_desc sd;
  struct gpl_desc pd;
  struct gfo_desc fd;
  char cmd[SWITCH_CMD_LEN], ip6_str[SWITCH_CMD_LEN];
  struct in_addr ip_addr;
  struct in6_addr ip6_addr;
  ns_nfm_ret_t ret;

  /* Initialise descriptors */
  memset(&sd, 0, sizeof (sd));
  memset(&pd, 0, sizeof (pd));
  memset(&fd, 0, sizeof (fd));

  /* check to see if we have one GERCHR */
  ret = nfm_get_gerchrs_present(&devices);
  if (NS_NFM_ERROR_CODE(ret) != NS_NFM_SUCCESS){
    syslog(LOG_DEST | LOG_INFO,
            "Unable to determine switch type. Error (%d,%d): %s",
            NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret),
            ns_nfm_error_string(ret));
    return (2);
  }
  if (devices != 1) {
    *swicthd = 1;
    *attached_card = 1;
    *portd = 1;
    *failoverd = 1;
    return (2);
  }

  gerchr_up = 0;
  *swicthd = 0;
  *attached_card = 0;
  *portd = 0;
  *failoverd = 0;

  /* open the swicthd daemon */
  rc = gerd_open(&sd, GE_AP_ID_PORTSTATS);
  if (rc != 0) {
    *swicthd = 1;
    gerchr_up = 1;
  } else {
    rc = gerd_close(&sd);
  }

  /* open the portd daemon */
  rc = gpsl_open(&pd, GPL_AP_ID_USER1);
  if (rc != 0) {
    *portd = 1;
    gerchr_up = 1;
  } else {
    rc = gpsl_close(&pd);
  }

  /* open the failoverd daemon */
  rc = gftw_open(&fd, GFO_AP_ID_USER1);
  if (rc != 0) {
    *failoverd = 1;
    gerchr_up = 1;
  } else {
    rc = gftw_close(&fd);
  }

  /* if we can't talk to all three daemons check the interface. */
  ret = nfm_get_gerchr_ip_addr(0, &ip_addr);
  if (ret == NS_NFM_SUCCESS)
      snprintf(cmd, SWITCH_CMD_LEN,
               "ping -c 1 %s > /dev/null 2>&1", inet_ntoa(ip_addr));
  else {
      /* we may use IPv6 addresses */
      ret = nfm_get_gerchr_ipv6_addr(0, &ip6_addr);
      if (ret != NS_NFM_SUCCESS)
          return gerchr_up;

      inet_ntop(AF_INET6, &ip6_addr, ip6_str, SWITCH_CMD_LEN);
      snprintf(cmd, SWITCH_CMD_LEN, "ping6 -c 1 %s > /dev/null 2>&1", ip6_str);
  }

  if ((*swicthd != 0) && (*portd != 0) && (*failoverd != 0)) {
      rc = system(cmd);
      if (rc != 0) {
          gerchr_up = 3;
      }
  }

  return (gerchr_up);
}
#endif /* GERCHR */
#else
static int check_nmsb_daemons(int *swicthd, int *attached_card, int *portd,
                              int *failoverd)
{
  *swicthd = 0;
  *attached_card = 0;
  *portd = 0;
  *failoverd = 0;
  return 0;
}
#endif /* NS_PLATFORM */

int main(int argc, char **argv)
{
  int c;
  char logname[LOGNAME_SIZE];
  pid_t mypid;
  FILE *f;
  unsigned int nfes;

  strncpy(daemon_name, basename(argv[0]), 32);
  snprintf(pidfile, 60, "/var/run/%s.pid", daemon_name);
  unlink(pidfile);

  /* parse our command line */
  opterr = 0;

  while ((c = getopt(argc, argv, "hdn")) != -1)
    switch (c) {
    case 'd':
      debug = 1;
      break;
    case 'n':
      no_daemonize = 1;
      break;
    case 'h':
    default:
      usage();
      exit(1);
    }


  /* open syslog */
  snprintf(logname, LOGNAME_SIZE, "%s", daemon_name);
  openlog(logname, 0, LOG_DEST);

  /* turn ourselves into a daemon with no terminal or files */
  if ((debug == 0) && (no_daemonize == 0)) {
    daemon(0, 0);
  }

  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);

  atexit(exitfunc);

  /* make our pidfile */
  mypid = getpid();
  f = fopen(pidfile, "w");
  if (!f) {
    exit(1);
  }
  if (fprintf(f, "%u\n", mypid) < 0) {
    exit(1);
  }
  fclose(f);

  syslog(LOG_DEST | LOG_INFO, "start");

  nfm_get_nfes_present(&nfes);

  /* start the processing loop. */
  while (notdone) {
    int switch_up, swicthd, attached_card, portd, failoverd;
    uint32_t heartbeats[MAX_NFES];
    int pd_up[MAX_NFES];
    int ep_up[MAX_NFES];
    int rd_up[MAX_NFES];
    int fd_up[MAX_NFES];
    unsigned int c;
    ns_nfm_ret_t nfm_return;
    ns_msg_return_t nfd_err;
    struct ns_msg_info_t nmi;
    int percent, pool;

    /* check to see if the NMSB/GERCHR card is running */
#ifdef GERCHR
    switch_type_t switch_type;
    nfm_return = nfm_get_switch_type(&switch_type);
    if (NS_NFM_ERROR_CODE(nfm_return) != NS_NFM_SUCCESS) {
        syslog(LOG_DEST | LOG_INFO,
               "Unable to determine switch type. Error (%d,%d): %s",
               NS_NFM_ERROR_CODE(nfm_return), NS_NFM_ERROR_SUBCODE(nfm_return),
               ns_nfm_error_string(nfm_return));
    }

    if (switch_type == SW_GERCHR)
      switch_up = check_gerchr_daemons(&swicthd, &attached_card, &portd,
              &failoverd);
    else if (switch_type == SW_NMSB)
#endif
    switch_up = check_nmsb_daemons(&swicthd, &attached_card, &portd,
            &failoverd);
    if (switch_up == 3) {
      /* The ARM on the NMSB/GERCHR could not be reached. This is the default
       * state before the card is configured. If it happens after the
       * NMSB/GERCHR has been configured no access to the daemons
       * is possible. */
      syslog(LOG_DEST | LOG_INFO,
             "The ARM on the NMSB/GERCHR could not be reached.");
    } else if (switch_up == 2) {
      /* This is usually a hardware problem. */
      syslog(LOG_DEST | LOG_INFO,
             "No NMSB/GERCHR hardware was found.");
    } else if (switch_up != 0) {
      /* A daemon isn't running, but the interface to talk to them is up. */
      if (swicthd != 0) {
        /*
         * This requires a restart of the NMSB/GERCHR daemons, usually as
         * part of a full NFM restart. No NMSB/GERCHR configuration changes
         * are possible without this daemon.
         */
        syslog(LOG_DEST | LOG_INFO, "swicthd not responding.");
      }
      if (portd != 0) {
        /*
         * This requires a restart of the NMSB/GERCHR daemons, usually as
         * part of a full NFM restart. Portstats and link state
         * information is not available without this daemon.
         */
        syslog(LOG_DEST | LOG_INFO, "portd not responding.");
      }
      if (failoverd != 0) {
        /*
         * This requires a restart of the NMSB/GERCHR daemons, usually as
         * part of a full NFM restart. The failoverd relay control is
         * not available without this daemon.
         */
        syslog(LOG_DEST | LOG_INFO, "failoverd not responding.");
      }
      /* This is usually a problem with the hardware. We don't
       * recognise the card type that is included with the NMSB/GERCHR. The
       * NMSB/GERCHR daemons will have minimal functionality.*/
      if (attached_card != 0) {
        syslog(LOG_DEST | LOG_INFO, "attached_card is unknown.");
      }
    }

    /* for each NFE card check a number of things */
    for (c = 0; c < nfes; c++) {
      /* See if we can talk to the platform daemon on the xscale and
       * check to see if the microengine heartbeats are still
       * ticking. */
      pd_up[c] = check_platform_daemon_and_me(c, &heartbeats[c]);
      if (pd_up[c] != 0) {
        if (check_NFE_ip(c) == 0) {
          /* The platform daemon is not respomding from the xscale. No
           * configuration changes can be made to the microcode. */
          syslog(LOG_DEST | LOG_INFO,
                 "The platform daemon for NFE %d did not respond.", c);
        } else {
          /* We're not able to talk to the NFE over IP. Either the NFE
           * has never been configured, or something bad has happened
           * to it. Recovery will probably require resetting the NFE
           * with a power cycle. */
          syslog(LOG_DEST | LOG_INFO,
                 "NFE %d is not respoding to IP requests.", c);
        }
      }
      if (heartbeats[c] != 0) {
        /* Some microengine is not running. This is likely to cause
         * problems with all traffic, and requires a full NFM restart
         * to recover. */
        syslog(LOG_DEST | LOG_INFO,
               "The microengine heartbeats for NFE %d did not tick. HB is %d",
               c, heartbeats[c]);
      }

      ep_up[c] = check_platform_endpoints(c);
      if (ep_up[c] != 0) {
          syslog(LOG_DEST | LOG_INFO,
                 "Endpoints are down according to counters on NFE %d.", c);
      }

      /* Check to see if the rules daemon is still running on the
       * host. */
      rd_up[c] = check_daemon("rulesd", c);
      if (rd_up[c] != 0) {
        /* Without a rules daemon you cannot change the rules on the
         * card. Whatever rules have already been programmed will
         * continue to run. It's possible to restart just the rules
         * daemon. */
        syslog(LOG_DEST | LOG_INFO,
               "The rules daemon for NFE %d is not running.", c);
      }
      /* check to see if the exception daemon is still running on the
       * host. */
      fd_up[c] = check_exceptiond();
      if (fd_up[c] != 0) {
        /* Without an exception daemon, fragmented traffic cannot be
         * processed.  Non-fragmented traffic will continue. */
        syslog(LOG_DEST | LOG_INFO,
               "The exception daemon for NFE %d is not running.", c);
      }
      /* Check the temperature on the NFE. */
      check_NFE_temp(c);

      /* Check to see if the NFM message daemon is running on all of
       * the NFEs.
       */
      nfm_return = nfm_ping_nfm_daemon(c);
      if (NS_NFM_ERROR_CODE(nfm_return) != NS_NFM_SUCCESS) {
        syslog(LOG_DEST | LOG_INFO,
               "The NFM daemon on NFE %d didn't respond. Error (%d,%d): %s",
               c, NS_NFM_ERROR_CODE(nfm_return),
               NS_NFM_ERROR_SUBCODE(nfm_return),
               ns_nfm_error_string(nfm_return));
        /* if the main error code indicates a general NFD error the
         * subcode contains the major NFD error code. */
        if (NS_NFM_ERROR_CODE(nfm_return) == NS_NFM_NFD) {
            nfd_err = NS_MSG_BUILD_ERROR_CODE(
                NS_NFM_ERROR_SUBCODE(nfm_return), 0);
            syslog(LOG_DEST | LOG_INFO, "  NFD error: %s\n",
                   ns_msg_error_string(nfd_err));
        }
      }
      /* Check to see if the TCAM daemon is running on all of
       * the NFEs.
       */
      nfm_return = nfm_ping_tcam_daemon(c);
      if (NS_NFM_ERROR_CODE(nfm_return) != NS_NFM_SUCCESS) {
        syslog(LOG_DEST | LOG_INFO,
               "The TCAM daemon on NFE %d didn't respond. Error (%d,%d): %s",
               c, NS_NFM_ERROR_CODE(nfm_return),
               NS_NFM_ERROR_SUBCODE(nfm_return),
               ns_nfm_error_string(nfm_return));
        /* if the main error code indicates a general NFD error the
         * subcode contains the major NFD error code. */
        if (NS_NFM_ERROR_CODE(nfm_return) == NS_NFM_NFD) {
            nfd_err = NS_MSG_BUILD_ERROR_CODE(
                NS_NFM_ERROR_SUBCODE(nfm_return), 0);
            syslog(LOG_DEST | LOG_INFO, "  NFD error: %s\n",
                   ns_msg_error_string(nfd_err));
        }
      }

      /* Query buffer information
       */
      nfm_get_nfd_msg_info(c, &nmi);
      for (pool=0; pool<NFD_BUFFER_POOL_COUNT; ++pool) {  // iterate through the 4 buffer pools
        if (nmi.buffer_pool[pool].size) {
          percent = (nmi.buffer_pool[pool].host_in_use * 100) / nmi.buffer_pool[pool].total;
          if (percent > NFD_POOL_HIGHWATER)
            syslog(LOG_DEST | LOG_INFO, "There may be a host buffer leak in pool %d on NFE %d.", pool, c);
          percent = (nmi.buffer_pool[pool].nfe_in_use * 100) / nmi.buffer_pool[pool].total;
          if (percent > NFD_POOL_HIGHWATER)
            syslog(LOG_DEST | LOG_INFO, "There may be an NFE buffer leak in pool %d on NFE %d.", pool, c);
        }
      }
    }
    sleep(1);
  }

  syslog(LOG_DEST | LOG_INFO, "stop");

  return 0;
}
