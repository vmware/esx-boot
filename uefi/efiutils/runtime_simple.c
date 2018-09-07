/*******************************************************************************
 * Copyright (c) 2016-2017 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * runtime_simple.c -- the safest RTS allocation policy.
 *
 * "simple" policy means relocating all RT regions by a single fixed offset,
 * maintaining the relative offset between all mappings. Additionally, the
 * quirk to present both new and old mappings to SetVirtualAddressMap is
 * coded in a way to accomodate broken UEFI implementations that access
 * memory outside of the UEFI map and expect the mappings for these regions
 * to be relocated as well.
 *
 * This is the only policy for machines like the Dell PowerEdge T320, and
 * generally the prefered policy for x86. The only reason why any other
 * policy might be used is if the relocated RT regions would not fit in
 * in the OS-specified RTS VA range.
 *
 * This is also the only policy that can be used to boot a kernel with the
 * old bit 17 RTS support.
 */

#include "efi_private.h"
#include "cpu.h"

/*-- simple_fill -------------------------------------------------------------
 *
 *      Fill out the passed UEFI memory map for the SetVirtualAddressMap,
 *      setting VirtualStart to addresses within the OS-specified RTS VA
 *      range.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *      OUT virtualMapSize: pointer to variable for storing memory map size.
 *
 * Results
 *      None.
 *
 * Side Effects
 *      Also updates the original UEFI memory from ExitBootServices with
 *      the new VAs for RT regions.
 *----------------------------------------------------------------------------*/
static void simple_fill(efi_info_t *efi_info,
                        EFI_MEMORY_DESCRIPTOR *vmap)
{
   uint32_t i;
   EFI_MEMORY_DESCRIPTOR *p;
   EFI_MEMORY_DESCRIPTOR *v;

   i = efi_info->num_descs;
   p = efi_info->mmap;
   v = vmap;
   while (i > 0) {
      if (p->Attribute & EFI_MEMORY_RUNTIME) {
         memcpy(v, p, efi_info->desc_size);

         v->VirtualStart = v->PhysicalStart + efi_info->rts_vaddr;
         p->VirtualStart = v->VirtualStart;
         Log(LOG_DEBUG, "simple RTS type=%u phys=%lx virt=%lx pgs=%x attr=%lx",
             v->Type, v->PhysicalStart, v->VirtualStart, v->NumberOfPages,
             v->Attribute);
         v = NextMemoryDescriptor(v, efi_info->desc_size);
      }
      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }
}

/*-- simple_supported --------------------------------------------------------
 *
 *      Return if this policy is supported, i.e. if the resulting RT VA layout
 *      will fit the OS specified VA region, and fill in the size of the
 *      UEFI memory map for the SetVirtualAddressMap on success.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *      OUT virtualMapSize: pointer to variable for storing memory map size.
 *
 * Results
 *      ERR_SUCCESS or ERR_UNSUPPORTED.
 *----------------------------------------------------------------------------*/
static int simple_supported(efi_info_t *efi_info,
                            uint64_t *virtualMapSize)
{
   uint32_t i;
   uint64_t map_size;
   EFI_MEMORY_DESCRIPTOR *p;

   /*
    * Figure out if this policy is supported for the passed
    * UEFI memory map and figure out the size of the virtual map
    * needed.
    */
   i = efi_info->num_descs;
   p = efi_info->mmap;
   map_size = 0;
   while (i > 0) {
      if (p->Attribute & EFI_MEMORY_RUNTIME) {
         map_size += efi_info->desc_size;
         if ((p->PhysicalStart +
              (p->NumberOfPages << EFI_PAGE_SHIFT)) >
             efi_info->rts_size) {
            return ERR_UNSUPPORTED;
         }
      }

      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }

   *virtualMapSize = map_size;
   return ERR_SUCCESS;
}

/*-- simple_pre ----------------------------------------------------------------
 *
 *      A firmware bug has been observed on Dell T320 machines, where
 *      SetVirtualAddressMap expects both the old identity and the new
 *      mappings to exist. Moreover, runtime drivers during SVAM make
 *      accesses to ranges outside of the UEFI memory map and expect
 *      these ranges to be relocated as well. All of this behavior
 *      violates the UEFI spec and means the only way we can make
 *      RTS relocation work is a) offset by the same simple offset
 *      b) create new PT mappings based on the old PT mappings, not
 *      UEFI map. Fortunately the offset used by ESX is large enough
 *      that physical and virtual ranges will not overlap.
 *
 *      Since we know rts_vaddr is supposed to begin on a PML4E region
 *      boundary (512G) we can use the trick of copying all valid
 *      PML4E entries by a simple offset.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *      IN vmap: unused
 *      IN virtualMapSize: unused
 *----------------------------------------------------------------------------*/
static void simple_pre(UNUSED_PARAM(efi_info_t *efi_info),
                       UNUSED_PARAM(EFI_MEMORY_DESCRIPTOR *vmap),
                       UNUSED_PARAM(uint64_t virtualMapSize))
{
#if defined(only_em64t) || defined(only_arm64)
   uint64_t *l4pt;
   uint64_t i, j;

   if ((efi_info->caps & EFI_RTS_CAP_OLD_AND_NEW) == 0) {
      return;
   }

   /*
    * We create the virtual mappings using the simple trick of copying the
    * L4PTEs that are used in the identity mapping into the part of the L4PT
    * that maps memory starting at rts_vaddr. This requires rts_vaddr to be
    * aligned on a PG_TABLE_LnE_SIZE(4) boundary.
    */
   EFI_ASSERT(efi_info->rts_vaddr % PG_TABLE_LnE_SIZE(4) == 0);

   l4pt = get_page_table_root();

   /*
    * Start i at the L4PTE for the beginning of the physical identity map (that
    * is, entry 0).  Start j at the L4PTE for rts_vaddr.  Copy corresponding
    * entries. We have already validated that none of the destination
    * entries were used.
    */
   for (i = 0, j = ((efi_info->rts_vaddr / PG_TABLE_LnE_SIZE(4)) &
                    (PG_TABLE_MAX_ENTRIES - 1));
        i < PG_TABLE_MAX_ENTRIES && j < PG_TABLE_MAX_ENTRIES;
        i++, j++) {
      PG_SET_ENTRY_RAW(l4pt, j, l4pt[i]);
   }
#endif
}

/*-- simple_post ------------------------------------------------------------
 *
 *      Clear out the duplicate L4PTEs created in simple_pre. If we
 *      don't do that, when relocate_page_tables makes a deep copy of the page
 *      tables, it will see the duplicate L4PTEs and copy the structure under
 *      them a second time, thus consuming twice as much memory.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *      IN vmap: unused.
 *      IN virtualMapSize: unused.
 *----------------------------------------------------------------------------*/
static void simple_post(UNUSED_PARAM(efi_info_t *efi_info),
                        UNUSED_PARAM(EFI_MEMORY_DESCRIPTOR *vmap),
                        UNUSED_PARAM(uint64_t virtualMapSize))
{
#if defined(only_em64t) || defined(only_arm64)
   uint64_t *l4pt;
   unsigned i, j;

   if ((efi_info->caps & EFI_RTS_CAP_OLD_AND_NEW) == 0) {
      return;
   }

   l4pt = get_page_table_root();

   /*
    * Start i at the L4PTE for the beginning of the physical identity map (that
    * is, entry 0).  Start j at the L4PTE for rts_vaddr.  Erase corresponding
    * entries that were copied from the identity map.
    */
   for (i = 0, j = ((efi_info->rts_vaddr / PG_TABLE_LnE_SIZE(4)) &
                    (PG_TABLE_MAX_ENTRIES - 1));
        i < PG_TABLE_MAX_ENTRIES && j < PG_TABLE_MAX_ENTRIES;
        i++, j++) {

      if ((l4pt[j] & PG_ATTR_PRESENT) != 0 &&
          l4pt[j] == l4pt[i]) {
         PG_SET_ENTRY_RAW(l4pt, j, 0);
      }
   }
#endif
}

rts_policy rts_simple = {
   "simple offset",
   simple_supported,
   simple_fill,
   simple_pre,
   simple_post,
   /* No unsupported platform quirks. */
   0,
   /*
    * OS must support simple policy (all kernels do, but rts_test
    * might wish to explicitly disable this policy to try
    * a different one).
    */
   EFI_RTS_CAP_RTS_SIMPLE,
};

rts_policy rts_simple_generic_quirk = {
   "simple offset with generic pre/post",
   simple_supported,
   simple_fill,
   rts_generic_pre,
   rts_generic_post,
   /* No unsupported platform quirks. */
   0,
   /*
    * OS must support simple policy (all kernels do, but rts_test
    * might wish to explicitly disable this policy to try
    * a different one).
    */
   EFI_RTS_CAP_RTS_SIMPLE_GQ,
};
