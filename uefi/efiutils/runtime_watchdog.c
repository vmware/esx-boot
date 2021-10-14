/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * runtime_watchdog.c -- Hardware runtime watchdog functions
 */

#include <string.h>
#include <ctype.h>
#include <crc.h>
#include <bootlib.h>
#include <libgen.h>
#include <stdlib.h>
#include "efi_private.h"

static RUNTIME_WATCHDOG_PROTOCOL *wdog = NULL;

/*-- set_runtime_watchdog ----------------------------------------------------
 *
 *      Set the runtime watchdog timer. Setting the Timeout parameter to
 *      zero disables the watchdog timer. The timer can be re-enabled by
 *      resetting it with a non-zero Timeout value.
 *
 * Parameters
 *      IN timeout: expiration time, in seconds
 *
 * Results
 *      EFI_SUCCESS, or a generic error status
 *----------------------------------------------------------------------------*/
int set_runtime_watchdog(unsigned int timeout)
{
   EFI_STATUS status;
   EFI_ASSERT(wdog != NULL);

   status = wdog->SetWatchdog(wdog, timeout);

   return error_efi_to_generic(status);
}


/*-- dump_runtime_watchdog -----------------------------------------------------
 *
 *      Dumps the runtime watchdog protocol info, if present.
 *
 * Parameters
 *      OUT minTimeoutSec: minimum timeout seconds
 *      OUT maxTimeoutSec: maximum timeout seconds
 *      OUT watchdogType: runtime watchdog type
 *      OUT baseAddr: protocol base address
 *
 * Results
 *      None
 *----------------------------------------------------------------------------*/
void dump_runtime_watchdog(unsigned int *minTimeoutSec,
                           unsigned int *maxTimeoutSec,
                           int *watchdogType,
                           unsigned int *baseAddr)
{
   Log(LOG_DEBUG, "inside dump_runtime_watchdog");
   EFI_ASSERT(wdog != NULL);

   Log(LOG_DEBUG, "past assert");
   *minTimeoutSec = wdog->MinTimeoutSeconds;
   *maxTimeoutSec = wdog->MaxTimeoutSeconds;
   *watchdogType = wdog->Type;
   *baseAddr = wdog->Base;

   Log(LOG_DEBUG, "done dumping");
}


/*-- init_runtime_watchdog -----------------------------------------------------
 *
 *      Initializes the runtime watchdog protocol, if present.
 *
 * Parameters
 *      None
 *
 * Results
 *      EFI_SUCCESS, or a generic error status
 *----------------------------------------------------------------------------*/
int init_runtime_watchdog(void)
{
   EFI_STATUS Status;
   EFI_GUID runtimeWatchdogProto = RUNTIME_WATCHDOG_PROTOCOL_GUID;

   Status = LocateProtocol(&runtimeWatchdogProto, (void **)&wdog);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   return ERR_SUCCESS;
}
