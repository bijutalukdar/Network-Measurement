/**
 * Copyright (C) 2008-2011 Netronome Systems, Inc.  All rights reserved.
 *
 * File:        nfm_sample_lb.c
 * Description: Sample application using the load balance API.
 */

#include "nfm_loadbalance.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <errno.h>

static void print_usage(const char* argv0)
{
  fprintf(stderr, "USAGE: %s [options]\n"
                  "\n"
                  "Options:\n"
                  " -p --print             Print the load balance group destination IDs\n"
                  " -s --set group:bitmask Set load balance <group> (0-23) to use destination/s in <bitmask>\n"
                  " -S --list group:list   Set load balance <group> (0-23) to use destination/s in <list>\n"
                  " -d --device n          Select NFE device (default 0, valid values 0-3)\n"
                  " -c --clear             Clear all 24 load balance groups\n"
                  "\nNote that the numerical arguments may be hexadecimal if prefixed by '0x'.\n"
                  "Multiple command line parameters may be combined to form a sequence of actions\n"
                  "executed left to right. The program will stop as soon as an error is encountered,\n"
                  "but all actions that have been successfully carried out up to that point remain\n"
                  "in effect. The entire command line sequence can therefore NOT be regarded as an\n"
                  "atomic operation.\n"
                  "\nExamples:\n"
                  "Set load balance group 3 to send to destination IDs 0,1,2,3,4,5:\n"
                  "%s -s 3:0x3f\n"
                  "Clear destinations for LB group 0 on device 0, and set LB group 1 to\n"
                  " send to dest IDs 0 and 2. Print the LB groups for device 0. Set the\n"
                  " destinations for LB group 0 on device 1 to send to IDs 0 to 15 and print groups:\n"
                  "%s -d 0 -s 0:0 -s 1:5 -p -d 1 -s 0:0xffff -p\n\n"
                  "Set group 0 to send to dest 0, group 1 to send to dest 1, ..., group 5 to dest 5:\n"
                  "%s -s 0:1 -s 1:2 -s 2:4 -s 3:8 -s 4:0x10 -s 5:0x20\n"
                  "Set group 0 to send to dest 0,1,2, group 1 to send to dest 3,4,5:\n"
                  "%s -s 0:7 -s 1:0x38\n"
                  "Clear groups, then set group 0 to send to dest 0,1,2, group 1 to send to\n"
                  " dest 3,4,5 (using the alternative list syntax):\n"
                  "%s -c -S 0:0,1,2 -S 1:3,4,5\n",
          argv0, argv0, argv0, argv0, argv0, argv0);
  exit(1);
}

static const struct option __long_options[] = {
  {"device",    1, 0, 'd'},
  {"print",     0, 0, 'p'},
  {"set",       1, 0, 's'},
  {"help",      0, 0, 'h'},
  {"clear",     0, 0, 'c'},
  {"list",      1, 0, 'S'},
  {0, 0, 0, 0}
};

int main(int argc, char **argv)
{
  int c;
  unsigned int device=0;
  uint32_t dests[NUM_LOAD_BALANCE_GROUPS];
  ns_nfm_ret_t ret;
  unsigned int i,j;
  char* opt;
  char* p;

  if (argc==1)
    print_usage(argv[0]);

  while ((c = getopt_long(argc, argv, "d:s:hpcS:", __long_options, NULL)) != -1) {
    switch (c) {
    case 'd':
      device = (unsigned int)strtoul(optarg,0,0);
      if (device > 3) {
        fprintf(stderr, "Device %d is out of range (0-3)\n\n", device);
        print_usage(argv[0]);
      }
      break;
    case 'c':
      {
        uint32_t d[NUM_LOAD_BALANCE_GROUPS];
        memset(d, 0, sizeof(d));
        ret=nfm_lb_set_dests(device, d);
        if (ret!=NS_NFM_SUCCESS) {
          fprintf(stderr, "Error clearing load balance group destinations: %s\n", ns_nfm_error_string(ret));
          exit(1);
        }
      }
      break;
    case 'p':
      ret=nfm_lb_get_dests(device, dests);
      if (ret!=NS_NFM_SUCCESS) {
        fprintf(stderr, "Error retrieving load balance group destinations from NFE: %s\n", ns_nfm_error_string(ret));
        exit(1);
      }
      printf("Load balance group destinations for NFE %u\n", device);
      for (i=0; i<NUM_LOAD_BALANCE_GROUPS; i++) {
        printf("Group %u:", i);
        if (!dests[i]) {
          printf(" No destinations defined\n");
        } else {
          for (j=0; j<32; j++) {
            if ((1<<j)&dests[i]) {
              printf(" %u", j);
            }
          }
          printf("\n");
        }
      }
      printf("\n");
      break;
    case 's':
      if (strspn(optarg, "0123456789:xabcdefABCDEF")!=strlen(optarg)) {
        print_usage(argv[0]);
      }
      opt=strchr(optarg, ':');
      if (!opt) {
        print_usage(argv[0]);
      }
      ++opt;
      errno=0;
      i=(unsigned int)strtoul(optarg,0,0);
      if (errno==EINVAL) {
        print_usage(argv[0]);
      }
      if (i>=NUM_LOAD_BALANCE_GROUPS) {
        print_usage(argv[0]);
      }
      errno=0;
      j=(unsigned int)strtoul(opt,0,0);
      if (errno==EINVAL) {
        print_usage(argv[0]);
      }
      ret=nfm_lb_set_group_dests(device, i, j);
      if (ret!=NS_NFM_SUCCESS) {
        fprintf(stderr, "Error setting load balance group %u to destinations 0x%x to NFE %u: %s\n", i, j, device, ns_nfm_error_string(ret));
        exit(1);
      }
      break;
    case 'S':
      if (strspn(optarg, "0123456789:xabcdefABCDEF,")!=strlen(optarg)) {
        print_usage(argv[0]);
      }
      opt=strchr(optarg, ':');
      if (!opt) {
        print_usage(argv[0]);
      }
      errno=0;
      i=(unsigned int)strtoul(optarg,0,0);
      if (errno==EINVAL) {
        print_usage(argv[0]);
      }
      if (i>=NUM_LOAD_BALANCE_GROUPS) {
        fprintf(stderr, "Invalid group ID (%u) specified, valid values are 0-%u\n\n", i, NUM_LOAD_BALANCE_GROUPS-1);
        print_usage(argv[0]);
      }
      opt=malloc(strlen(opt+1)+1);
      strcpy(opt, strchr(optarg, ':')+1);
      dests[i]=0;
      p=strtok(opt, ",");
      while (p) {
        errno=0;
        j=(unsigned int)strtoul(p,0,0);
        if (errno==EINVAL) {
          print_usage(argv[0]);
        }
        if (j>31) {
          fprintf(stderr, "Invalid destination ID (%u) specified, valid values are 0-31\n\n", j);
          print_usage(argv[0]);
        }
        dests[i]|=(1<<j);
        p=strtok(0, ",");
      }
      free(opt);
      ret=nfm_lb_set_group_dests(device, i, dests[i]);
      if (ret!=NS_NFM_SUCCESS) {
        fprintf(stderr, "Error setting load balance group %u to destinations 0x%x to NFE %u: %s\n", i, dests[i], device, ns_nfm_error_string(ret));
        exit(1);
      }
      break;
    case 'h':
    default:
      print_usage(argv[0]);
    }
  }

  if (optind != argc) {
    fprintf(stderr, "Unexpected parameter on command line.\n");
    print_usage(argv[0]);
  }

  return 0;
}
