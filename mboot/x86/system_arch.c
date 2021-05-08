/*******************************************************************************
 * Copyright (c) 2017,2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * system_arch.c -- Various architecture-specific system routines.
 */

#include <cpu.h>
#include <cpuid.h>
#include <boot_services.h>
#include <system_int.h>

#include "mboot.h"

static bool is_intel_skylake;
CPUIDRegs cpuid0;
CPUIDRegs cpuid1;

void
__GET_CPUID(unsigned int input, CPUIDRegs *regs)
{
   __get_cpuid(input, &regs->eax, &regs->ebx, &regs->ecx, &regs->edx);
}

unsigned int
CPUID_EFFECTIVE_FAMILY(unsigned int v) /* %eax from CPUID with %eax=1. */
{
   unsigned int f = CPUID_GET(1, EAX, FAMILY, v);
   return f != CPUID_FAMILY_EXTENDED ? f : f +
      CPUID_GET(1, EAX, EXTENDED_FAMILY, v);
}

bool
CPUID_FAMILY_IS_P6(unsigned int eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_P6;
}

unsigned int
CPUID_EFFECTIVE_MODEL(unsigned int v) /* %eax from CPUID with %eax=1. */
{
   unsigned int m = CPUID_GET(1, EAX, MODEL, v);
   unsigned int em = CPUID_GET(1, EAX, EXTENDED_MODEL, v);
   return m + (em << 4);
}

bool
CPUID_UARCH_IS_SKYLAKE(unsigned int v) // IN: %eax from CPUID with %eax=1.
{
   unsigned int model = 0;

   if (!CPUID_FAMILY_IS_P6(v)) {
      return false;
   }

   model = CPUID_EFFECTIVE_MODEL(v);

   return (model == CPUID_MODEL_SKYLAKE_4E    ||
           model == CPUID_MODEL_SKYLAKE_55    ||
           model == CPUID_MODEL_SKYLAKE_5E    ||
           model == CPUID_MODEL_CANNONLAKE_66 ||
           model == CPUID_MODEL_KABYLAKE_8E   ||
           model == CPUID_MODEL_KABYLAKE_9E);
}

bool
CPUID_IsRawVendor(CPUIDRegs *id0, const char* vendor)
{
   return (id0->ebx == *(const unsigned int *)(uintptr_t) (vendor + 0) &&
           id0->ecx == *(const unsigned int *)(uintptr_t) (vendor + 4) &&
           id0->edx == *(const unsigned int *)(uintptr_t) (vendor + 8));
}

bool
CPUID_IsVendorIntel(CPUIDRegs *id0)
{
   return CPUID_IsRawVendor(id0, CPUID_INTEL_VENDOR_STRING);
}

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
 *      List all the memory ranges that may not be used by the bootloader.
 *
 * Parameters
 *      IN mmap:  pointer to the system memory map
 *      IN count: number of entries in the memory map
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
