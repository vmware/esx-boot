/*******************************************************************************
 * Copyright (c) 2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * init_arch.c -- Architecture-specific EFI Firmware init/cleanup functions.
 */

#include <bootlib.h>
#include "efi_private.h"

/*-- sanitize_page_tables ------------------------------------------------------
 *
 *      Validate and transform MMU configuration to the state expected by
 *      allocate_page_tables and relocate_page_tables1/2.
 *
 *      Ensuring we have the full 4 levels of page tables present.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int sanitize_page_tables(void)
{
   unsigned i;
   uint64_t tcr;
   uint64_t pdbr;
   uint64_t pdbr_flags;
   EFI_STATUS Status;
   unsigned max_level = mmu_max_levels();
   unsigned max_level_entries = mmu_max_entries(max_level);

   Log(LOG_DEBUG, "%s: MMU %u SCTLR = 0x%lx TCR = 0x%lx MAIR = 0x%lx TTBR = %p\n",
       el_is_hyp() ? "EL2" : "EL1", is_paging_enabled(),
       get_sctlr(), get_tcr(), get_mair(), get_page_table_root());
   Log(LOG_DEBUG, "T0SZ = 0x%x Max Levels = %u\n", mmu_t0sz(),
       mmu_max_levels());
   Log(LOG_DEBUG, "L4 Max Entries: %u\n", mmu_max_entries(4));
   Log(LOG_DEBUG, "L3 Max Entries: %u\n", mmu_max_entries(3));
   Log(LOG_DEBUG, "L2 Max Entries: %u\n", mmu_max_entries(2));
   Log(LOG_DEBUG, "L1 Max Entries: %u\n", mmu_max_entries(1));

   if (!mmu_supported_configuration()) {
      Log(LOG_ERR, "MMU configuration unsupported\n");
      return ERR_UNSUPPORTED;
   }

   get_page_table_reg(&pdbr);
   pdbr_flags = pdbr & ~PG_ROOT_ADDR_MASK;
   pdbr &= PG_ROOT_ADDR_MASK;
   tcr = get_tcr();

   if ((pdbr & PG_OFF_MASK) != 0) {
      /*
       * Arm allows the top level directory to have less
       * than 512 entries, in which case it is not page aligned
       * anymore. Realign.
       */
      uint64_t aligned_pdbr;

      Status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                 1, &aligned_pdbr);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }

      for (i = 0; i < max_level_entries; i++) {
         PG_SET_ENTRY_RAW(((uint64_t *) aligned_pdbr), i,
                          *(((uint64_t *) pdbr) + i));
      }

      for (; i < PG_TABLE_MAX_ENTRIES; i++) {
         PG_SET_ENTRY_RAW(((uint64_t *) aligned_pdbr), i, 0);
      }

      pdbr = (uint64_t) aligned_pdbr;
   } else {
      /*
       * Clear any excess.
       */
      for (i = max_level_entries; i < PG_TABLE_MAX_ENTRIES; i++) {
         PG_SET_ENTRY_RAW(((uint64_t *) pdbr), i, 0);
      }
   }

   if (max_level < 3) {
      uint64_t l3;
      /*
       * pdbr is an L2.
       */
      Status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                 1, &l3);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }

      PG_SET_ENTRY_RAW(((uint64_t *) l3), 0, pdbr | PG_ATTR_TYPE_TABLE);
      for (i = 1; i < PG_TABLE_MAX_ENTRIES; i++) {
         PG_SET_ENTRY_RAW(((uint64_t *) l3), i, 0);
      }

      pdbr = l3;
   }

   if (max_level < 4) {
      uint64_t l4;
      /*
       * pdbr is an L3.
       */
      Status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData,
                                 1, &l4);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }

      PG_SET_ENTRY_RAW(((uint64_t *) l4), 0, pdbr | PG_ATTR_TYPE_TABLE);
      for (i = 1; i < PG_TABLE_MAX_ENTRIES; i++) {
         PG_SET_ENTRY_RAW(((uint64_t *) l4), i, 0);
      }

      pdbr = l4;
   }

   /*
    * Restore ASID if used.
    */
   pdbr |= pdbr_flags;

   DSB();
   tcr &= ~TCR_ELx_TnSZ_MASK;
   tcr |= TCR_ELx_TnSZ_MIN_WITH_PML4_LOOKUP;

   if (el_is_hyp()) {
      MSR(tcr_el2, tcr);
   } else {
      MSR(tcr_el1, tcr);
   }
   set_page_table_reg(&pdbr);

   return ERR_SUCCESS;
}
