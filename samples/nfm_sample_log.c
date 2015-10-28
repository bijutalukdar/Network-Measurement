/**
 * Copyright (C) 2006-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * @file nfm_sample_log.c
 * Sample application to demonstrate the use of the NFM logging subsystem
 * and integration with the host logging subsystems (e.g. syslog)
 */

#include <stdlib.h>
#include <syslog.h>
#include "ns_log.h"

/**
 * Callback function upon logging events used to populate system log
 */
static void log_event(ns_log_lvl_e lvl, const char* file, unsigned int line, const char* func, const char* str)
{
  int sysloglevel;

  switch (lvl) {
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
  if (sysloglevel != -1)
    syslog(sysloglevel,"File:%s Line:%d Func:%s Message:%s",file,line,func,str);
}

 int main()
{
  ns_nfm_ret_t ret;
  int oldmask;

  ///Open a connection to the host system logger user logfile. Log the process ID as well. Delay until first syslog
  ///On a debian system this will be /var/log/user.log
  openlog("nfm_sample_log",LOG_ODELAY|LOG_PID,LOG_USER);
  ///Log only debug messages and error messages in syslog. Store the old log mask to restore later.
  oldmask = setlogmask(LOG_MASK(LOG_DEBUG)|LOG_MASK(LOG_ERR));
  ///Enable NFM logging to console
  if (NS_NFM_ERROR_CODE(ret = ns_log_init(NS_LOG_CONSOLE)) != NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Could not initialize log (%d,%d): %s", NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret), ns_nfm_error_string(ret));
    return EXIT_FAILURE;
  }
  ///Set the ns log level to everything from debug level and up
  if (NS_NFM_ERROR_CODE(ret = ns_log_lvl_set(NS_LOG_LVL_DEBUG)) != NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Could not set log level (%d,%d): %s", NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret), ns_nfm_error_string(ret));
    return EXIT_FAILURE;
  }
  ///Register a logging callback function that is called on all logger events
  if (NS_NFM_ERROR_CODE(ret = ns_log_set_cb(log_event)) != NS_NFM_SUCCESS) {
    NS_LOG_ERROR("Could register callback function (%d,%d): %s", NS_NFM_ERROR_CODE(ret), NS_NFM_ERROR_SUBCODE(ret), ns_nfm_error_string(ret));
    return EXIT_FAILURE;
  }
  ///Log at level error and write a newline at the end
  NS_LOG_PRINT(NS_LOG_LVL_ERROR,"Will display in syslog (A file in /var/log tree)");
  ///Log an error and do not write a newline at the end, don't send file name and function call back.
  NS_LOG_RAW(NS_LOG_LVL_ERROR,"This will also display in syslog (A file in the /var/log tree)");
  ///Log an info message that will not display in syslog
  NS_LOG_PRINT(NS_LOG_LVL_INFO,"This will not display in syslog");
  ///Reset old syslog log mask
  setlogmask(oldmask);
  ///Close connection to system log
  closelog();
  return EXIT_SUCCESS;
}
