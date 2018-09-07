/*******************************************************************************
 * Copyright (c) 2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * runtime_generic.c -- Shared pre/post-SetVirtualAddressMap quirks for
 *                      RTS mapping policies.
 */

#include "efi_private.h"
#include "cpu.h"

#if defined(only_em64t) || defined(only_arm64)
/*-- get_l1e_flags -------------------------------------------------------------
 *
 *      For an LPN, return the leaf mapping flags, which include
 *      the execute, write and caching attributes, translated to
 *      L1 mapping flags.
 *
 * Parameters
 *      IN l4pt: page table root
 *      IN lpn: logical page number to return l1pt for.
 *
 * Results
 *      L1 mapping flags.
 *----------------------------------------------------------------------------*/
uint64_t get_l1e_flags(uint64_t *l4pt, uint64_t lpn)
{
   uint64_t flags;
   uint64_t entry;
   unsigned level;
   uint64_t *pt = l4pt;

   for (level = 4; level != 1; level--) {
      entry = PG_GET_ENTRY(pt, level, lpn);
      EFI_ASSERT((entry & PG_ATTR_PRESENT) != 0);
      if (PG_IS_LARGE(entry)) {
         /*
          * 1GB or 2MB page.
          *
          * N.B. 512GB (PML4E entries) are not supported (yet? ever? on x86, but
          * are supported on ARM64), but it's still safe since PG_IS_LARGE
          * bit is defined as "must be 0" for PML4E today.
          */
         goto large_entry;
      }

      pt = PG_ENTRY_TO_PG(entry);
   }

   EFI_ASSERT(level == 1);
   entry = PG_GET_ENTRY(pt, 1, lpn); /* PTE */
   EFI_ASSERT((entry & PG_ATTR_PRESENT) != 0);

large_entry:
   flags = PG_ENTRY_TO_PAGE_FLAGS(entry);
   return flags;
}

/*-- alloc_lnpt ------------------------------------------------------------------
 *
 *      Create and install a missing page table for Ln+1 for an LPN.
 *
 * Parameters
 *      IN pt: page table at Ln
 *      IN n: level of pt
 *      IN lpn: logical page number to allocate for.
 *      IN dirCacheFlags: additional flags to use constructing
 *                        the PTE
 *      OUT newpt: allocated and zeroed page table
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int alloc_lnpt(uint64_t *pt, unsigned n, uint64_t lpn,
                      uint64_t dirCacheFlags, uint64_t **newpt)
{
   int status;
   uint64_t addr;

   status = alloc(&addr, PAGE_SIZE, ALIGN_PAGE, ALLOC_ANY);
   if (status != ERR_SUCCESS) {
      return status;
   }

   memset((void *) addr, 0, PAGE_SIZE);
   PG_SET_ENTRY(pt, n, lpn, addr >> EFI_PAGE_SHIFT,
                (dirCacheFlags | PG_ATTR_PRESENT |
                 PG_ATTR_W | PG_ATTR_A));

   *newpt = (uint64_t *) addr;
   return ERR_SUCCESS;
}

/*-- get_l1pt ------------------------------------------------------------------
 *
 *      Return the L1 page table corresponding to the LPN, given
 *      the page table root.
 *
 *      Any missing L3, L2 and L1 tables are created.
 *
 * Parameters
 *      IN l4pt: page table root
 *      IN lpn: logical page number to return l1pt for.
 *      IN dirCacheFlags: additional flags to use constructing
 *                        any missing tables on the way.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_l1pt(uint64_t *l4pt, uint64_t lpn,
                    uint64_t dirCacheFlags, uint64_t **l1pt)
{
   int status;
   uint64_t entry;
   unsigned level;
   uint64_t *pt = l4pt;

   for (level = 4; level != 1; level--) {
      entry = PG_GET_ENTRY(pt, level, lpn);
      if ((entry & PG_ATTR_PRESENT) == 0) {
         status = alloc_lnpt(pt, level, lpn, dirCacheFlags, &pt);
         if (status != ERR_SUCCESS) {
            return status;
         }
      } else {
         pt = PG_ENTRY_TO_PG(entry);
      }
   }

   *l1pt = pt;
   return ERR_SUCCESS;
}
#endif

/*-- rts_generic_pre ----------------------------------------------------------
 *
 *      A firmware bug may exist where SetVirtualAddressMap expects both
 *      the old identity and the new  mappings to exist. This behavior
 *      violates the UEFI spec and means the only way we can make
 *      RTS relocation work is to  create new PT mappings based on the virtual
 *      map. Fortunately the offset used by ESX is large enough
 *      that physical and virtual ranges will not overlap.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *      IN vmap: UEFI map describing RT mappings
 *      IN virtualMapSize: size of vmap
 *----------------------------------------------------------------------------*/
void rts_generic_pre(UNUSED_PARAM(efi_info_t *efi_info),
                     UNUSED_PARAM(EFI_MEMORY_DESCRIPTOR *vmap),
                     UNUSED_PARAM(uint64_t virtualMapSize))
{
#if defined(only_em64t) || defined(only_arm64)
   int status;
   uintptr_t pt_root_reg;
   uint64_t *l4pt;
   uint64_t dirCacheFlags;
   EFI_MEMORY_DESCRIPTOR *desc;
   EFI_MEMORY_DESCRIPTOR *lastDesc = (EFI_MEMORY_DESCRIPTOR *)
      (((uintptr_t) vmap) +
       virtualMapSize);

   if ((efi_info->caps & EFI_RTS_CAP_OLD_AND_NEW) == 0) {
      return;
   }

   /*
    * Need this to construct intermediate page tables, making
    * the assumption that the same memory type is used to
    * look up subsequent page table levels.
    */
   get_page_table_reg(&pt_root_reg);
   dirCacheFlags = PG_DIR_CACHING_FLAGS(pt_root_reg);
   l4pt = get_page_table_root();

   for (desc = vmap;
        desc < lastDesc;
        desc = NextMemoryDescriptor(desc, efi_info->desc_size)) {
      uint64_t *l1pt;
      uint64_t flags;
      unsigned indexL1;
      uint64_t nextLpn, lastLpn, nextMpn, lastMpn;

      nextLpn = desc->VirtualStart >> EFI_PAGE_SHIFT;
      lastLpn = nextLpn + desc->NumberOfPages;
      nextMpn = desc->PhysicalStart >> EFI_PAGE_SHIFT;
      lastMpn = nextMpn + desc->NumberOfPages;

      /*
       * Computing flags is not trivial (e.g. PAT) and
       * we don't want to get it wrong. Fortunately we
       * can simply lift existing leaf flags for any mapping
       * because it has to be mapped linearly now. Leaf, not L1,
       * since the firmware may have used large pages.
       */
      flags = get_l1e_flags(l4pt, nextMpn);

      while (nextMpn < lastMpn) {
         status = get_l1pt(l4pt, nextLpn, dirCacheFlags, &l1pt);
         if (status != ERR_SUCCESS) {
            Log(LOG_WARNING, "L1PT allocation failure");
            return;
         }

         for (indexL1 = PG_LPN_2_L1OFF(nextLpn);
              (indexL1 < PG_TABLE_MAX_ENTRIES &&
               nextLpn < lastLpn);
              indexL1++) {
            EFI_ASSERT((PG_GET_ENTRY(l1pt, 1, nextLpn) &
                        PG_ATTR_PRESENT) == 0);

            PG_SET_ENTRY(l1pt, 1, nextLpn,
                         nextMpn, flags);

            nextLpn++;
            nextMpn++;

            if (nextMpn == lastMpn) {
               break;
            }
         }
      }
   }
#endif
}

/*-- rts_generic_post --------------------------------------------------------
 *
 *      Clear out the L4PTEs created in generic_pre, unhooking mappings
 *      created. We don't bother freeing up memory.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *      IN vmap: UEFI map describing RT mappings
 *      IN virtualMapSize: size of vmap
 *----------------------------------------------------------------------------*/
void rts_generic_post(UNUSED_PARAM(efi_info_t *efi_info),
                      UNUSED_PARAM(EFI_MEMORY_DESCRIPTOR *vmap),
                      UNUSED_PARAM(uint64_t virtualMapSize))
{
#if defined(only_em64t) || defined(only_arm64)
    uint64_t *l4pt;
    EFI_MEMORY_DESCRIPTOR *desc;
    EFI_MEMORY_DESCRIPTOR *lastDesc = (EFI_MEMORY_DESCRIPTOR *)
       (((uintptr_t) vmap) +
        virtualMapSize);

    if ((efi_info->caps & EFI_RTS_CAP_OLD_AND_NEW) == 0) {
       return;
    }

    l4pt = get_page_table_root();
    for (desc = vmap;
         desc < lastDesc;
         desc = NextMemoryDescriptor(desc, efi_info->desc_size)) {
       uint64_t nextLpn, lastLpn;

       nextLpn = desc->VirtualStart >> EFI_PAGE_SHIFT;
       lastLpn = nextLpn + desc->NumberOfPages;

       while (nextLpn < lastLpn) {
          if ((PG_GET_ENTRY(l4pt, 4, nextLpn) & PG_ATTR_PRESENT) != 0) {
             PG_SET_ENTRY(l4pt, 4, nextLpn, 0, 0);
          }
          nextLpn++;
       }
    }
#endif
}
