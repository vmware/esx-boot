/*******************************************************************************
 * Copyright (c) 2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include "efi_private.h"
#include "cpu.h"

/*
 * runtime_sparse.c -- the sparse mapping RTS allocation policy.
 *
 * Like "simple offset", but avoids the hole before start of UEFI RTS region
 * and first RT mapping. This is generally preferable to "simple", but the
 * quirk to present both new and old mappings to SetVirtualAddressMap relies
 * on UEFI properly describing all memory ranges in the UEFI memory map
 * and not have bugs the rely on accesses to non-RT ranges at relocated
 * addresses.
 */


/*-- get_smallest_rt_pa ------------------------------------------------------
 *
 *      Return the smallest runtime PA base seen in the memory map.
 *
 *      Note that the UEFI memory map is not sorted and we
 *      can't sort it since the firmware may run in a mode where
 *      it enforces relative ordering of the passed SVA entries relative
 *      to the original memory map.
 *
 * Parameters
 *      IN efi_info: EFI information.
 *
 * Results
 *      Smallest PA base for the runtime descriptors seen in the UEFI mmap.
 *----------------------------------------------------------------------------*/
static uint64_t get_smallest_rt_pa(efi_info_t *efi_info)
{
   uint64_t pa = (uint64_t) -1UL;
   uint32_t i = efi_info->num_descs;
   EFI_MEMORY_DESCRIPTOR *p = efi_info->mmap;

   while (i > 0) {
      if ((p->Attribute & EFI_MEMORY_RUNTIME) != 0) {
         if (p->PhysicalStart < pa) {
            pa = p->PhysicalStart;
         }
      }

      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }

   return pa;
}


/*-- sparse_supported --------------------------------------------------------
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
static int sparse_supported(efi_info_t *efi_info,
                            uint64_t *virtualMapSize)
{
   uint32_t i;
   uint64_t map_size;
   EFI_MEMORY_DESCRIPTOR *p;
   uint64_t pa_offset;

   /*
    * Figure out if this policy is supported for the passed
    * UEFI memory map and figure out the size of the virtual map
    * needed.
    */
   i = efi_info->num_descs;
   p = efi_info->mmap;
   map_size = 0;
   pa_offset = get_smallest_rt_pa(efi_info);

   while (i > 0) {
      if ((p->Attribute & EFI_MEMORY_RUNTIME) != 0) {
         map_size += efi_info->desc_size;
         if ((p->PhysicalStart - pa_offset +
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

/*-- sparse_fill -------------------------------------------------------------
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
static void sparse_fill(efi_info_t *efi_info,
                        EFI_MEMORY_DESCRIPTOR *vmap)
{
   uint32_t i;
   uint64_t pa_offset;
   EFI_MEMORY_DESCRIPTOR *p;
   EFI_MEMORY_DESCRIPTOR *v;

   i = efi_info->num_descs;
   p = efi_info->mmap;
   v = vmap;
   pa_offset = get_smallest_rt_pa(efi_info);

   while (i > 0) {
      if ((p->Attribute & EFI_MEMORY_RUNTIME) != 0) {
         memcpy(v, p, efi_info->desc_size);

         v->VirtualStart = v->PhysicalStart - pa_offset +
            efi_info->rts_vaddr;
         p->VirtualStart = v->VirtualStart;
         Log(LOG_DEBUG, "sparse RTS type=%u phys=%"PRIx64" virt=%"PRIx64
             " pgs=%"PRIx64" attr=%"PRIx64"",
             v->Type, v->PhysicalStart, v->VirtualStart, v->NumberOfPages,
             v->Attribute);
         v = NextMemoryDescriptor(v, efi_info->desc_size);
      }
      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }
}

rts_policy rts_sparse = {
   "sparse",
   sparse_supported,
   sparse_fill,
   rts_generic_pre,
   rts_generic_post,
   /*
    * The pre/post mapper code uses the UEFI memory map, not
    * source page table, thus this policy cannot be used on machines
    * which are known to access memory outside of any UEFI ranges.
    */
   EFI_RTS_UNKNOWN_MEM,
   /*
    * OS must support sparse policy (i.e. OS does not rely on specific
    * old-RTS layout).
    */
   EFI_RTS_CAP_RTS_SPARSE,
};
