/*******************************************************************************
 * Copyright (c) 2015-2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * runtime.c -- EFI runtime services handoff.
 */

#include <string.h>
#include "efi_private.h"

/*
 * These are ordered in terms of safety and likelihood of
 * not triggering some horrible bugs in out-of-spec
 * UEFI implementations.
 *
 * A policy is chosen if:
 *    a) the resulting RT VA layout fits into the OS-provided VA region
 *    b) there are no platform quirks blacklisting the policy (or quirks
 *       are disabled)
 *    c) the platform/OS capabilities match policy requirements.
 *
 * If no policies match we boot without RT support.
 */
static rts_policy *policies[] = {
   &rts_simple,
   &rts_simple_generic_quirk,
   &rts_sparse,
   &rts_compact,
   &rts_contig,
   /*
    * Must be last.
    */
   NULL,
};

#define last_pol(p) ((p) == NULL)

/*-- can_old_and_new -----------------------------------------------------------
 *
 *     Return ERR_SUCCESS if none of the 1:1 RT mappings collide with the
 *     OS-provided VA region reserved for RTS mappings. We check this
 *     so that we opportunistically map both 1:1 and calculated VA mappings
 *     prior to the SetVirtualAddressMap, to work around possible runtime driver
 *     bugs.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *
 * Results
 *      ERR_SUCCESS or ERR_UNSUPPORTED.
 *----------------------------------------------------------------------------*/
static int can_old_and_new(UNUSED_PARAM(efi_info_t *efi_info))
{
#if defined(only_em64t) || defined(only_arm64)
   uint64_t *l4pt;
   uint64_t entry;
   uint64_t rts_start = efi_info->rts_vaddr;
   uint64_t rts_end = rts_start + efi_info->rts_size;
   EFI_MEMORY_DESCRIPTOR *p = efi_info->mmap;
   uint32_t i = efi_info->num_descs;

   while (i > 0) {
      if (p->Attribute & EFI_MEMORY_RUNTIME) {
         uint64_t last_addr = p->PhysicalStart +
            (p->NumberOfPages << EFI_PAGE_SHIFT);
         if ((p->PhysicalStart >= rts_start &&
              p->PhysicalStart < rts_end) ||
             (last_addr > rts_start &&
              last_addr < rts_end)) {
            /*
             * Overlap with RTS region detect, we
             * will not be able to create new mappings
             * before call to SetVirtualAddressMap.
             */
            Log(LOG_WARNING, "Old/new conflict; skipping temp map quirk");
            return ERR_UNSUPPORTED;
         }
      }

      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }

   /*
    * Now validate that the firmware page tables don't
    * have valid-looking values for the PML4 entry
    * corresponding to the RTS region.
    *
    * This check used to live in runtime_generic.c pre
    * call before, but this is the proper place to
    * correctly set EFI_RTS_CAP_OLD_AND_NEW state for
    * all policies (including runtime_simple.c, which
    * does not use runtime_generic.c hooks).
    */
   l4pt = get_page_table_root();

   /*
    * Skip this workaround if the page tables are mapped read-only.
    * (Checks only the l4pt page itself.)
    * We've seen this on recent Apple firmware (PR 1713949).
    * Fortunately Apple doesn't need the workaround.
    * Note: This isn't possible on x86 anymore, as at this point
    * we are on our own copy of the PTs and we have cleared all
    * read-only flags.
    */
   if (PG_IS_READONLY(get_l1e_flags(l4pt, (uint64_t)l4pt >> EFI_PAGE_SHIFT))) {
      Log(LOG_DEBUG, "Page tables are read-only; skipping temp map quirk");
      return ERR_UNSUPPORTED;
   }

   for (i = ((rts_start / PG_TABLE_LnE_SIZE(4)) &
             (PG_TABLE_MAX_ENTRIES - 1));
        i <= (((rts_end - 1) / PG_TABLE_LnE_SIZE(4)) &
              (PG_TABLE_MAX_ENTRIES - 1));
        i++) {

      entry = l4pt[i];
      if ((entry & PG_ATTR_PRESENT) != 0) {
         Log(LOG_DEBUG, "Unexpected contents 0x%lx for PML4 entry for 0x%lx",
             entry, i * PG_TABLE_LnE_SIZE(4));
         if (arch_is_arm64) {
            return ERR_UNSUPPORTED;
         } else {
            /*
             * We have seen a number of x86 boxes that leave garbage in L4 page
             * table entries beyond the end of physical memory.  Examples
             * include an IBM box (PR 1698684), a Dell Edge Gateway 5000, and
             * an AMD Myrtle prototype (PR 1792733 update #49).  In theory the
             * firmware could really be using the vmkernel's RTS VA range for
             * something, but it's extremely unlikely.  So if we see a present
             * L4PTE there, just clear the entry and continue.  Clearing the
             * entry is important for every policy besides "simple", as
             * otherwise we'd interpret the garbage as a real pointer to an L3
             * PT.
             */
            PG_SET_ENTRY_RAW(l4pt, i, 0);
         }
      }
   }

   return ERR_SUCCESS;
#else
   return ERR_UNSUPPORTED;
#endif
}

/*-- relocate_runtime_services -------------------------------------------------
 *
 *     Enable runtime services use by the kernel to be booted.  Assumes the
 *     kernel will map EFI runtime memory regions to virtual addresses at an
 *     offset of efi_info.rts_vaddr from their physical addresses.
 *
 *     Design note: Calling SetVirtualAddressMap here in the bootloader, where
 *     the original EFI memory map is still valid and active, saves us the
 *     difficulty of recreating the necessary parts of the original EFI map in
 *     the kernel just to be able to make this one call to inform runtime
 *     services of the new map.
 *
 * Parameters
 *      IN efi_info:  EFI information.
 *      IN no_rts:    if RTS should be disabled
 *      IN no_quirks: if system quirks should be ignored
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      No call to runtime services may be made after a call to this function
 *      until the kernel has set up the new mappings.
 *----------------------------------------------------------------------------*/
int relocate_runtime_services(efi_info_t *efi_info, bool no_rts, bool no_quirks)
{
   struct rts_policy **pol = policies;
   EFI_MEMORY_DESCRIPTOR *virtualMap;
   EFI_STATUS Status;
   int status;
   uint64_t buf;
   uint64_t virtualMapSize;

   if (no_rts) {
      Log(LOG_INFO, "UEFI runtime services support is disabled");
      return ERR_SUCCESS;
   }

   if (!no_quirks) {
      if ((efi_info->quirks & EFI_RTS_UNSUPPORTED) != 0) {
         Log(LOG_INFO, "UEFI runtime services support is disabled on quirk");
         return ERR_SUCCESS;
      }
   }

   if (efi_info->rts_vaddr == 0) {
      Log(LOG_DEBUG, "Kernel does not support UEFI runtime services");
      return ERR_SUCCESS;
   } else {
      Log(LOG_DEBUG,
          "OS wants UEFI runtime services at 0x%"PRIx64" (size 0x%"PRIx64")",
          efi_info->rts_vaddr, efi_info->rts_size);
   }

   if (!no_quirks) {
      if (can_old_and_new(efi_info) == ERR_SUCCESS) {
         Log(LOG_DEBUG, "Can accommodate old and new RTS mappings");
         efi_info->caps |= EFI_RTS_CAP_OLD_AND_NEW;
      }

      if ((efi_info->caps & EFI_RTS_CAP_OLD_AND_NEW) == 0 &&
          (efi_info->quirks & EFI_RTS_OLD_AND_NEW) != 0) {
         /*
          * Quirks say both mappings must be present, but
          * we know it won't work.
          */
         Log(LOG_INFO,
             "Booting without RTS support (can't apply quirk 0x%"PRIx64")",
             EFI_RTS_OLD_AND_NEW);
         return ERR_SUCCESS;
      }
   }

   virtualMapSize = 0;
   status = ERR_UNSUPPORTED;
   while (!last_pol(*pol)) {
      if ((!no_quirks || ((*pol)->incompat_efi_quirks &
                          efi_info->quirks) == 0) &&
          ((*pol)->efi_caps & efi_info->caps) == (*pol)->efi_caps) {
         status = (*pol)->supported(efi_info, &virtualMapSize);
         if (status == ERR_SUCCESS) {
            Log(LOG_INFO, "Using '%s' UEFI RTS mapping policy", (*pol)->name);
            break;
         }
      }

      pol++;
   }

   if (status != ERR_SUCCESS) {
      /*
       * No supported RTS mapping policy, sorry.
       */
      Log(LOG_INFO, "Booting without RTS support (no supported policies)");
      return ERR_SUCCESS;
   }

   /*
    * At this point efi_malloc is no longer usable because boot
    * services have been shut down.  But mboot's alloc is usable
    * because blacklist_bootloader_mem has been run; see alloc.c.
    */
   status = alloc(&buf, virtualMapSize, sizeof(uint64_t), ALLOC_ANY);
   if (status != ERR_SUCCESS) {
      Log(LOG_WARNING,
          "Failed to allocate virtual address map for UEFI runtime services");
      return status;
   }
   virtualMap = UINT64_TO_PTR(buf);
   memset(virtualMap, 0, virtualMapSize);

   (*pol)->fill(efi_info, virtualMap);

   EFI_ASSERT(rs != NULL);
   EFI_ASSERT(rs->SetVirtualAddressMap != NULL);

   if (!no_quirks) {
      (*pol)->pre_quirk(efi_info, virtualMap, virtualMapSize);
   }

   Status = rs->SetVirtualAddressMap(virtualMapSize,
                                     efi_info->desc_size,
                                     efi_info->version,
                                     virtualMap);
   if (EFI_ERROR(Status)) {
      Log(LOG_WARNING,
          "Failed to set virtual address map for UEFI runtime services");
   }

   if (!no_quirks) {
      if ((efi_info->caps & EFI_RTS_CAP_RTS_DO_TEST) != 0 &&
          (efi_info->caps & EFI_RTS_CAP_OLD_AND_NEW) != 0) {
         VOID *dummy = NULL;

         Log(LOG_INFO, "Trying simple RTS test");
         Status = rs->ConvertPointer(EFI_OPTIONAL_PTR, &dummy);
         Log(LOG_INFO, "We came back from RTS test!");
      }

      (*pol)->post_quirk(efi_info, virtualMap, virtualMapSize);
   }

   /*
    * Runtime Services now expects the virtual mappings to be set up, so it
    * cannot be used again until vmkBoot has done that.
    */
   rs = NULL;

   return error_efi_to_generic(Status);
}
