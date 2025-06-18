/*******************************************************************************
 * Copyright (c) 2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * fdt.c -- Arch-agnostic code to handle Flattened Device Tree.
 */

#include <boot_services.h>
#include <fdt_vmware.h>
#include "mboot.h"

/*-- fdt_blacklist_memory ------------------------------------------------------
 *
 *      Blacklist ranges reserved by the FDT blob.
 *
 * Parameters
 *      IN fdt: the FDT.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int fdt_blacklist_memory(void *fdt)
{
   int i;
   int node;
   int subnode;
   int num_rsv;

   /*
    * Ideally every range should already be in the UEFI
    * memory map, thus all the blacklisting calls below should
    * fail.
    */

   node = fdt_path_offset(fdt, "/reserved-memory");
   if (node < 0) {
      return ERR_SUCCESS;
   }

   subnode = fdt_first_subnode(fdt, node);
   while (subnode >= 0) {
      uint64_t base;
      int size = fdt_get_reg(fdt, subnode, "reg", &base);
      if (size > 0) {
         Log(LOG_INFO, "Blacklisting /reserved-memory 0x%"PRIx64"-0x%"PRIx64,
             base, base + size - 1);
         (void) blacklist_runtime_mem(base, size);
      }
      subnode = fdt_next_subnode(fdt, subnode);
   }

   /*
    * Now let's also reserve everything in the memory reservation
    * block. Yes, this is different from /reserved-memory node, and
    * no, that's of course not confusing at all.
    */
   num_rsv = fdt_num_mem_rsv(fdt);
   for (i = 0; i < num_rsv; i++) {
      uint64_t base;
      uint64_t size;

      if (!fdt_get_mem_rsv(fdt, i, &base, &size)) {
         Log(LOG_INFO, "Blacklisting memrsv 0x%"PRIx64"-0x%"PRIx64,
             base, base + size - 1);
         (void) blacklist_runtime_mem(base, size);
      }
   }

   return ERR_SUCCESS;
}
