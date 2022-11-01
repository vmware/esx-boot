/*******************************************************************************
 * Copyright (c) 2017,2019-2020,2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * system_arch.c -- Various architecture-specific system routines.
 */

#include <cpu.h>
#include <boot_services.h>
#include <system_int.h>

#include "mboot.h"

static bool is_intel_skylake;
CPUIDRegs cpuid0;
CPUIDRegs cpuid1;

void
check_cpu_quirks(void)
{
   __GET_CPUID(0, &cpuid0);
   __GET_CPUID(1, &cpuid1);
   is_intel_skylake = (CPUID_IsVendorIntel(&cpuid0) &&
                       CPUID_UARCH_IS_SKYLAKE(cpuid1.eax));
}

/*-- system_arch_blacklist_memory ----------------------------------------------
 *
 *      Blacklist architecture-specific memory ranges.
 *
 * Parameters
 *      None.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int system_arch_blacklist_memory(void)
{
   int status;

   status = blacklist_runtime_mem(0, LOW_IBM_PC_MEGABYTE - 0);

   if (status != ERR_SUCCESS) {
      return status;
   }

   if (is_intel_skylake)  {
      Log(LOG_DEBUG, "Intel Skylake Arch detected, applying HLE workaround.\n");

      status = blacklist_runtime_mem(SKYLAKE_HLE_BLACKLIST_MA_LOW,
                                     (SKYLAKE_HLE_BLACKLIST_MA_HIGH -
                                      SKYLAKE_HLE_BLACKLIST_MA_LOW));

      if (status != ERR_SUCCESS) {
         Log(LOG_DEBUG, "Unable to apply HLE workaround.\n");
      }
   }

   return status;
}
