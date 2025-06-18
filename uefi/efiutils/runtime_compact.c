/*******************************************************************************
 * Copyright (c) 2016 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include "efi_private.h"
#include "cpu.h"

/*
 * runtime_compact.c -- the compact mapping RTS allocation policy.
 *
 * Compact policy tries to squish things together, only keeping
 * relative offsets between RuntimeDxe code/data regions in cases
 * where it is possible that non-64 bit relocation could be used
 * due to build/toolchain bugs.
 */

/*-- compact_supported -------------------------------------------------------
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
static int compact_supported(efi_info_t *efi_info,
                             uint64_t *virtualMapSize)
{
   uint32_t i;
   uint64_t map_size;
   EFI_MEMORY_DESCRIPTOR *p;
   EFI_MEMORY_DESCRIPTOR *prev;
   uint64_t next_va;

   /*
    * Figure out if this policy is supported for the passed
    * UEFI memory map and figure out the size of the virtual map
    * needed.
    */
   i = efi_info->num_descs;
   p = efi_info->mmap;
   map_size = 0;
   prev = NULL;
   next_va = efi_info->rts_vaddr;
   while (i > 0) {
      if (p->Attribute & EFI_MEMORY_RUNTIME) {
         map_size += efi_info->desc_size;

         if (prev != NULL &&
             (p->Type == EfiRuntimeServicesData ||
              p->Type == EfiRuntimeServicesCode) &&
             (prev->Type == EfiRuntimeServicesData ||
              prev->Type == EfiRuntimeServicesCode) &&
             (p->PhysicalStart - prev->PhysicalStart) < MAX_UINT32) {
            /*
             * If there's less than 4GB of a VA gap between two RT
             * regions, there could be an implicit dependence due
             * to PE/COFF non-IMAGE_REL_BASED_DIR64 fixups.
             *
             * What we're trying to avoid is end up relocating
             * same sections of the same RuntimeDxe by different
             * offsets, as that works only in a very specific
             * way of building EFI drivers that we can't rely on.
             * We could make this logic smarter by matching
             * RT entries against EFI_LOADED_IMAGE_PROTOCOLs for
             * Runtime DXEs, but UEFI usually doesn't pick
             * wholly random addresses for loading DXEs so
             * these are all going to be in some block of
             * memory that's only a few hundred megs in size.
             */
            next_va += p->PhysicalStart - prev->PhysicalStart -
               (prev->NumberOfPages << EFI_PAGE_SHIFT);
         }

         next_va += p->NumberOfPages << EFI_PAGE_SHIFT;
         prev = p;
      }

      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }

   if ((next_va - efi_info->rts_vaddr) > efi_info->rts_size) {
      return ERR_UNSUPPORTED;
   }

   *virtualMapSize = map_size;
   return ERR_SUCCESS;
}

/*-- compact_fill ------------------------------------------------------------
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
static void compact_fill(efi_info_t *efi_info,
                         EFI_MEMORY_DESCRIPTOR *vmap)
{
   uint32_t i;
   EFI_MEMORY_DESCRIPTOR *p;
   EFI_MEMORY_DESCRIPTOR *v;
   EFI_MEMORY_DESCRIPTOR *prev;
   uint64_t next_va;

   i = efi_info->num_descs;
   p = efi_info->mmap;
   v = vmap;
   prev = NULL;
   next_va = efi_info->rts_vaddr;
   while (i > 0) {
      if (p->Attribute & EFI_MEMORY_RUNTIME) {
         memcpy(v, p, efi_info->desc_size);

         if (prev != NULL &&
             (p->Type == EfiRuntimeServicesData ||
              p->Type == EfiRuntimeServicesCode) &&
             (prev->Type == EfiRuntimeServicesData ||
              prev->Type == EfiRuntimeServicesCode) &&
             (p->PhysicalStart - prev->PhysicalStart) < MAX_UINT32) {
            next_va += p->PhysicalStart - prev->PhysicalStart -
               (prev->NumberOfPages << EFI_PAGE_SHIFT);
         }

         v->VirtualStart = next_va;
         next_va += p->NumberOfPages << EFI_PAGE_SHIFT;
         prev = p;

         p->VirtualStart = v->VirtualStart;
         Log(LOG_DEBUG, "compact RTS type=%u phys=%"PRIx64" virt=%"PRIx64
             " pgs=%"PRIx64" attr=%"PRIx64"",
             v->Type, v->PhysicalStart, v->VirtualStart, v->NumberOfPages,
             v->Attribute);
         v = NextMemoryDescriptor(v, efi_info->desc_size);
      }
      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }
}

rts_policy rts_compact = {
   /*
    * Compact policy tries to squish things together, only keeping
    * relative offsets between RuntimeDxe code/data regions in cases
    * where it is possible that non-64 bit relocation could be used.
    */
   "compact",
   compact_supported,
   compact_fill,
   rts_generic_pre,
   rts_generic_post,
   /*
    * The pre/post mapper code uses the UEFI memory map, not
    * source page table, thus this policy cannot be used on machines
    * which are known to access memory outside of any UEFI ranges.
    */
   EFI_RTS_UNKNOWN_MEM,
   /*
    * OS must support compact policy (i.e. OS does not rely on specific
    * old-RTS layout).
    */
   EFI_RTS_CAP_RTS_COMPACT,
};
