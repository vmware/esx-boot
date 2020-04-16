/*******************************************************************************
 * Copyright (c) 2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include "efi_private.h"
#include "cpu.h"

/*
 * runtime_contig.c -- the "contiguous" mapping RTS allocation policy.
 *
 * The "contiguous" policy is the last thing we try. It is
 * supposed to work with non-buggy UEFI implementations. Similar
 * to "compact" but we squish all the RT regions together.
 */

/*-- contig_supported --------------------------------------------------------
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
static int contig_supported(efi_info_t *efi_info,
                            uint64_t *virtualMapSize)
{
   uint32_t i;
   uint64_t map_size;
   EFI_MEMORY_DESCRIPTOR *p;
   uint64_t next_va;

   /*
    * Figure out if this policy is supported for the passed
    * UEFI memory map and figure out the size of the virtual map
    * needed.
    */
   i = efi_info->num_descs;
   p = efi_info->mmap;
   map_size = 0;
   next_va = efi_info->rts_vaddr;
   while (i > 0) {
      if (p->Attribute & EFI_MEMORY_RUNTIME) {
         map_size += efi_info->desc_size;
         next_va += p->NumberOfPages << EFI_PAGE_SHIFT;
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

/*-- contig_fill -------------------------------------------------------------
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
static void contig_fill(efi_info_t *efi_info,
                        EFI_MEMORY_DESCRIPTOR *vmap)
{
   uint32_t i;
   EFI_MEMORY_DESCRIPTOR *p;
   EFI_MEMORY_DESCRIPTOR *v;
   uint64_t next_va;

   i = efi_info->num_descs;
   p = efi_info->mmap;
   v = vmap;
   next_va = efi_info->rts_vaddr;
   while (i > 0) {
      if (p->Attribute & EFI_MEMORY_RUNTIME) {
         memcpy(v, p, efi_info->desc_size);
         v->VirtualStart = next_va;
         next_va += p->NumberOfPages << EFI_PAGE_SHIFT;

         p->VirtualStart = v->VirtualStart;
         Log(LOG_DEBUG, "contig RTS type=%u phys=%"PRIx64" virt=%"PRIx64
             " pgs=%"PRIx64" attr=%"PRIx64"",
             v->Type, v->PhysicalStart, v->VirtualStart, v->NumberOfPages,
             v->Attribute);
         v = NextMemoryDescriptor(v, efi_info->desc_size);
      }
      p = NextMemoryDescriptor(p, efi_info->desc_size);
      i--;
   }
}

rts_policy rts_contig = {
   "contiguous",
   contig_supported,
   contig_fill,
   rts_generic_pre,
   rts_generic_post,
   /*
    * The pre/post mapper code uses the UEFI memory map, not
    * source page table, thus this policy cannot be used on machines
    * which are known to access memory outside of any UEFI ranges.
    */
   EFI_RTS_UNKNOWN_MEM,
   /*
    * OS must support contig policy (i.e. OS does not rely on specific
    * old-RTS layout).
    */
   EFI_RTS_CAP_RTS_CONTIG,
};
