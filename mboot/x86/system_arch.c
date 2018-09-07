/*******************************************************************************
 * Copyright (c) 2017 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * system_arch.c -- Various architecture-specific system routines.
 */

#include <cpu.h>
#include <boot_services.h>

#include "mboot.h"

/*
 * PC compatibles have BIOS and option card ROMs here,
 * "low" RAM, partially used by BIOS, VGA RAM.
 */
#define LOW_IBM_PC_MEGABYTE 0x100000ULL

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

   status = blacklist_runtime_mem(0, LOW_IBM_PC_MEGABYTE);
   if (status != ERR_SUCCESS) {
      return status;
   }

   return ERR_SUCCESS;
}
