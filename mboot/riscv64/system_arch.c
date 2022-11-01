/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * system_arch.c -- Various architecture-specific system routines.
 */

#include <boot_services.h>
#include <fdt_vmware.h>
#include "mboot.h"

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
   void *fdt;
   int fdt_error;

   if (get_fdt(&fdt) != ERR_SUCCESS) {
      /*
       * No FDT is okay. Server systems use ACPI.
       */
      return ERR_SUCCESS;
   }

   fdt_error = fdt_check_header(fdt);
   if (fdt_error != 0) {
      Log(LOG_ERR, "Bad FDT header: %s", fdt_strerror(fdt_error));
      return ERR_UNSUPPORTED;
   }

   status = fdt_blacklist_memory(fdt);
   if (status != ERR_SUCCESS) {
      return ERR_SUCCESS;
   }

   if (!fdt_match_system(fdt, "sifive,freedom-u74-arty")) {
      /*
       * OpenSBI firmware lives at [0x80000000,0x8001FFFF],
       * and accesses to this range cause an abort. This range should
       * have been reserved and isn't on the BeagleV. Firmware bug.
       */
      blacklist_runtime_mem(0x80000000, 0x20000);
   }

   return ERR_SUCCESS;
}

void check_cpu_quirks(void)
{
   return;
}
