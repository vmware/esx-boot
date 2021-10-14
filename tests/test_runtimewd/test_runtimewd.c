/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_runtimewd.c -- tests fdt-related functionality
 *
 *   test_runtimewd [-t]
 *
 *      OPTIONS
 *         -t <timeout_sec>  Set runtime watchdog for timeout_sec seconds
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <bootlib.h>
#include <boot_services.h>

/*-- parse_args ----------------------------------------------------------------
 *
 *      Parse test_runtimewd command line options.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *      OUT timeout_sec : pointer to the timeout seconds
 *
 * Results
 *      ERR_SUCCESS, or a generic error status
 *----------------------------------------------------------------------------*/
static int parse_args(int argc, char **argv, unsigned int *timeoutSec)
{
   int opt;

   if (argc == 0 || argv == NULL || argv[0] == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if (argc > 1) {
      optind = 1;
      do {
         opt = getopt(argc, argv, "t:h");
         switch (opt) {
            case -1:
               break;
            case 't':
               if (atoi(optarg) < 0) {
                  Log(LOG_ERR, "Invalid negative timeout sec.");
                  return ERR_SYNTAX;
               }
               *timeoutSec = atoi(optarg);
               break;
            case 'h':
            case '?':
            default:
               Log(LOG_ERR, "Usage: %s [-t timeout seconds]", argv[0]);
               return ERR_SYNTAX;
         }
      } while (opt != -1);
   }

   return ERR_SUCCESS;
}


/*-- main ----------------------------------------------------------------------
 *
 *      test_runtimewd main function
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv) {
   int status;
   unsigned int timeoutSec = 0;
   unsigned int minTimeoutSec;
   unsigned int maxTimeoutSec;
   int watchdogType;
   unsigned int baseAddr;

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = parse_args(argc, argv, &timeoutSec);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = init_runtime_watchdog();

   if (status != ERR_SUCCESS) {
      Log(LOG_INFO, "No runtime watchdog detected.\n");
      return status;
   }

   dump_runtime_watchdog(&minTimeoutSec, &maxTimeoutSec,
                         &watchdogType, &baseAddr);

   Log(LOG_INFO, "Runtime watchdog detected.\n");
   Log(LOG_INFO, "Min timeout seconds: %u\n", minTimeoutSec);
   Log(LOG_INFO, "Max timeout: %u\n", maxTimeoutSec);
   Log(LOG_INFO, "Watchdog type: %u.\n", watchdogType);
   Log(LOG_INFO, "Watchdog base address: 0x%x\n", baseAddr);

   if (timeoutSec == 0) {
      Log(LOG_INFO,
         "Setting runtime watchdog for 0 seconds. Disabling watchdog.\n");
   } else {
      Log(LOG_INFO, "Setting runtime watchdog for %u seconds.\n", timeoutSec);
   }

   status = set_runtime_watchdog(timeoutSec);

   return status;
}
