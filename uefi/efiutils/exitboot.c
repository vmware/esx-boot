/*******************************************************************************
 * Copyright (c) 2008-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * exitboot.c -- EFI Firmware functions to exit boot services.
 * (Split from init.c.)
 */

#include <string.h>
#include <ctype.h>
#include <crc.h>
#include <bootlib.h>
#include <libgen.h>
#include "efi_private.h"

#if defined(only_em64t) || defined(only_arm64)
static EFI_MEMORY_DESCRIPTOR *pt_reloc_cached_mmap;
static UINTN pt_reloc_cached_mmap_size;
static UINTN pt_reloc_cached_mmap_desc_size;
static uintptr_t pt_reloc_cached_mmap_memtop;

static void set_memtop(void)
{
   unsigned i;
   unsigned max_entries = pt_reloc_cached_mmap_size /
      pt_reloc_cached_mmap_desc_size;
   EFI_MEMORY_DESCRIPTOR *MMap = pt_reloc_cached_mmap;

   pt_reloc_cached_mmap_memtop = 0;
   for (i = 0; i < max_entries; i++) {
      uintptr_t bottom = MMap->PhysicalStart;
      uintptr_t top = bottom + (MMap->NumberOfPages << EFI_PAGE_SHIFT);

      if (top > pt_reloc_cached_mmap_memtop) {
         pt_reloc_cached_mmap_memtop = top;
      }
      MMap = NextMemoryDescriptor(MMap, pt_reloc_cached_mmap_desc_size);
   }
}

/*-- va_is_usable_ram ----------------------------------------------------------
 *
 *      Check whether an address is safe to assume to be RAM when
 *      copying page tables.  Used to avoid copying garbage page
 *      tables from non-RAM addresses, and to avoid mapping non-RAM as
 *      writable and executable.
 *
 * Parameters
 *      IN  va:            memory address
 *      OUT in_memory_map: true if address is in the UEFI memory map
 *
 * Results
 *      true if the address is safe to assume to be RAM
 *----------------------------------------------------------------------------*/
static bool va_is_usable_ram(uintptr_t va, bool *in_memory_map)
{
   unsigned i;
   unsigned max_entries = pt_reloc_cached_mmap_size /
      pt_reloc_cached_mmap_desc_size;
   EFI_MEMORY_DESCRIPTOR *MMap = pt_reloc_cached_mmap;
   bool good_ram = false;

   if (va >= pt_reloc_cached_mmap_memtop) {
      *in_memory_map = false;
      return false;
   }

   for (i = 0; i < max_entries; i++) {
      uintptr_t bottom = MMap->PhysicalStart;
      uintptr_t top = bottom + (MMap->NumberOfPages << EFI_PAGE_SHIFT);

      if (va >= bottom && va < top) {
         switch (MMap->Type) {
            /*
             * This list is ordered exactly as the
             * EFI_MEMORY_TYPE enum.
             */
         case EfiReservedMemoryType:
            good_ram = false; // paranoia; this type could be anything
            break;
         case EfiLoaderCode:
         case EfiLoaderData:
         case EfiBootServicesCode:
         case EfiBootServicesData:
            good_ram = true;
            break;
         case EfiRuntimeServicesCode:
         case EfiRuntimeServicesData:
            /*
             * We want to clean any RO/XN bits here, otherwise we
             * might crash inside gRT->SetVirtualAddressMap on some
             * implementations (e.g. AArch64 AMI Aptio).
             */
            good_ram = true;
            break;
         case EfiConventionalMemory:
            good_ram = true;
            break;
         case EfiUnusableMemory:
            good_ram = false;
            break;
         case EfiACPIReclaimMemory:
         case EfiACPIMemoryNVS:
            good_ram = true;
            break;
         case EfiMemoryMappedIO:
               /*
                * Okay the next two are Itanic-only, but for
                * consistency-sake I'll keep them,
                */
         case EfiMemoryMappedIOPortSpace:
         case EfiPalCode:
            good_ram = false;
            break;
         case EfiPersistentMemory:
            good_ram = true;
            break;
         default:
            good_ram = false;
            break;
         }

         /*
          * Found a matching range.
          */
         break;
      }

      MMap = NextMemoryDescriptor(MMap, pt_reloc_cached_mmap_desc_size);
   }

   if (i == max_entries) {
      if (in_memory_map != NULL) {
         *in_memory_map = false;
      }

      /*
       * Anything not in the memory map cannot be normal RAM.
       */
      return false;
   }

   /*
    * Found in memory map.
    */
   if (in_memory_map != NULL) {
      *in_memory_map = true;
   }
   return good_ram;
}


/*
 * On 64-bit UEFI, we create new page tables for use after ExitBootServices,
 * by copying the existing tables with modifications.  New page tables are
 * needed for three main reasons:
 *  (1) The existing tables may map some memory as non-writable or
 *       non-executable, that we will be reusing to copy and perhaps
 *       execute boot modules (PR 1900114).
 *  (2) The existing tables may themselves be mapped as non-writable,
 *      preventing them from being modified in-place (PR 1713949).
 *  (3) The existing tables may be in EfiBootServicesData memory that we will
 *      be reusing.  In particular, if any boot module is linked to load at
 *      a fixed address, we must ensure the page tables don't wind up at that
 *      address (PR 2170718).  On x86 the ESXBootInfo "kernel" (vmkBoot) is
 *      currently linked at a fixed address.  (That could be changed in the
 *      future, but mboot needs to be backward compatible -- a newer mboot must
 *      be able to boot an older system -- so we still have to handle the fixed
 *      address case.)
 *
 * We must move the page tables twice to address all these issues.  In the
 * first phase, we move them temporarily into memory that is known to be
 * writeable because we have UEFI allocate it as EfiLoaderData memory.  In the
 * second phase, we allocate "safe" memory with mboot's own allocator, after
 * space for the boot modules has been allocated and move the tables again.
 * This second move deals with the possibility that UEFI's allocator may have
 * returned memory that one of our boot modules must use.
 */

/*-- traverse_page_tables_rec --------------------------------------------------
 *
 *      Traverse the page tables recursively. If a buffer is provided, re-create
 *      the visited tables in this buffer (and adjust all internal pointers).
 *
 *      This function must first be called with buffer == NULL, in order to
 *      retrieve the amount of memory that is needed to hold all the page tables
 *      (assuming they will be written contiguously in memory).
 *
 *      Once this memory is allocated, the function can be called a second time
 *      with a pointer to the allocated buffer.
 *
 *      This function assumes that the page tables are identity mapped.  In
 *      other words, the physical addresses contained in the page tables being
 *      copied, and the physical addresses of the destination buffers, can
 *      simply be used as pointers.  There is no need to map these physical
 *      addresses in order to convert them to pointers.
 *
 *      This function does the following sanity checks:
 *
 *      1. If the page table entry being copied does not have the Present bit
 *      set, it is not copied. (The physical address contained in the entry,
 *      which could be pointing to the page table one level down, or could be
 *      pointing to data, is not validated.)
 *      2. If the page table entry describes a small or large page, and the
 *      mapping is not VA == PA, it is not copied, as it is either a garbage
 *      mapping or an alias (some firmware may map the RAM additionally at
 *      a high VA). Note: this allows mapping areas beyond RAM (e.g.
 *      MMIO like framebuffer BARs and UARTs).
 *      3. If the page table entry points to a following page table level,
 *      and the address of the next page table is not covered by a RAM
 *      entry in the UEFI memory map, the entry is skipped, as it must be a
 *      garbage entry (RAM is always mapped with VA == PA, so the page table
 *      appears to be outside of valid memory).
 *      4. If what whatever reason a page table corresponding to an address
 *      and level is empty (either because it was empty, or because all
 *      entries in it were considered invalid and ignored), then the
 *      page table is not copied, and the referencing page table entry
 *      to it is not copied either.
 *
 *      This code assumes 64-bit page tables (not 32-bit). It also assumes
 *      that 4 page table levels are used, and that PML4 has 512 entries
 *      like every other level. On Arm, sanitize_page_tables() is used
 *      to meet these requirements.
 *
 * Parameters:
 *      IN table:   pointer to the source page table
 *      IN level:   hierarchy level (4 = PML4, 1 = PT)
 *      IN vaddr:   first virtual address mapped by this table
 *      IN pa_mask: bits to be masked off PTEs to compute the PA
 *      IN hierarchical_attrs: bits to be OR'ed into every page or
 *                             block entry, since they were originally
 *                             implied by hierarchical page table attrs
 *                             that are going to be cleaed.
 *      OUT buffer: pointer to the output buffer, previously set to all zeroes.
 *
 * Results
 *      The number of page table visited during the traversal or 0 if
 *      the page table at level is empty or considered as empty.
 *----------------------------------------------------------------------------*/
static size_t traverse_page_tables_rec(uint64_t *table, int level,
                                       uintptr_t vaddr, uint64_t pa_mask,
                                       uint64_t hierarchical_attrs,
                                       uint64_t *buffer, uint64_t *buffer_end)
{
   unsigned i;
   size_t table_count = 1;
   size_t valid_entries = 0;
   uint64_t pa_mask_lg = pa_mask | PG_ATTR_LARGE_MASK;
   bool in_memory_map;
   bool is_usable_ram;

   /*
    * On the second pass, we might be past the end of the buffer because we're
    * about to scan a page table with no valid entries, which the first pass
    * determined we don't need to preserve and therefore didn't allocate space
    * for (item 4 in the header comment).  Return early in that case to avoid
    * overwriting memory beyond the end of the buffer (PR 2229147).
    */
   if (buffer != NULL && buffer >= buffer_end) {
      return 0;
   }

   for (i = 0; i < PG_TABLE_MAX_ENTRIES; i++) {
      uint64_t next_vaddr = vaddr + PG_TABLE_LnE_SIZE(level) * i;
      size_t traverse_count;
      uint64_t entry;
      uint64_t entry_paddr;

      entry = table[i];
      if (buffer != NULL) {
         PG_SET_ENTRY_RAW(buffer, i, 0);
      }

      if ((entry & PG_ATTR_PRESENT) == 0) {
          continue;
      }

      if (PG_IS_LARGE(level, entry)) {
         entry_paddr = entry & ~pa_mask_lg;
      } else {
         entry_paddr = entry & ~pa_mask;
      }

      is_usable_ram = va_is_usable_ram(next_vaddr, &in_memory_map);

      if (PG_IS_LARGE(level, entry) || level == 1) {
         if (entry_paddr != next_vaddr) {
            /*
             * The large or small page did not have VA == PA. Must be an
             * alias mapping or garbage.
             */
            if (in_memory_map) {
               /*
                * Do not log ranges outside of the UEFI memory map,
                * because this will seriously impact boot times on a
                * number of systems: e.g. Macs.
                */
               Log(LOG_DEBUG, "VA 0x%"PRIx64": Ignoring L%d E%d because PTE "
                   "0x%"PRIx64" points to non-matching PA 0x%"PRIx64,
                   next_vaddr, level, i, entry, entry_paddr);
            }

            continue;
         }

         if (buffer != NULL) {
            if (is_usable_ram) {
               /*
                * We may be relocating boot modules into pages that UEFI had
                * previously used for other purposes and protected against write
                * or execute access.  Ensure all pages are writable and
                * executable.
                */
               PG_SET_ENTRY_RAW(buffer, i,
                                PG_CLEAN_NOEXEC(
                                   PG_CLEAN_READONLY(
                                      /*
                                       * Hierarchical page attributes, on
                                       * architectures where supported, are only
                                       * used for forcing read-only and XN, so
                                       * we don't have to OR entry with
                                       * hierarchical_attrs, but will anyway to
                                       * stay correct in the general case that
                                       * hierarchical_attrs ever includes more
                                       * attributes.
                                       */
                                      (entry | hierarchical_attrs))));
            } else {
               /*
                * Don't touch the mapping attributes for anything that
                * doesn't look like normal RAM. (e.g. it could be MMIO),
                * but apply any hierarchical attributes implied by
                * traversed page tables (since those are going to always be
                * cleaned out)
                */
               PG_SET_ENTRY_RAW(buffer, i, entry | hierarchical_attrs);
            }
         }

         valid_entries++;
      } else {
         uint64_t *next_table = UINT_TO_PTR(entry_paddr);
         uint64_t *next_buf;

         if (!va_is_usable_ram(PTR_TO_UINT(next_table), NULL)) {
            /*
             * We have something that looks like a pointer to
             * a page table directory, but it's obviously corrupt
             * garbage, because it is not pointing to what we know
             * to be RAM.
             */
            Log(LOG_DEBUG, "VA 0x%"PRIx64": Ignoring L%d E%d because PTE "
                "0x%"PRIx64" points to table outside RAM at PA 0x%"PRIx64,
                next_vaddr, level, i, entry, PTR_TO_UINT(next_table));
            continue;
         }

         if (buffer != NULL) {
            next_buf = buffer + table_count * PG_TABLE_MAX_ENTRIES;
         } else {
            next_buf = NULL;
         }

         traverse_count = traverse_page_tables_rec(next_table, level - 1,
            next_vaddr, pa_mask,
            hierarchical_attrs | PG_TABLE_XD_RO_2_PAGE_ATTRS(entry),
            next_buf, buffer_end);

         if (traverse_count != 0) {
            if (buffer != NULL) {
               /*
                * Clean any hierarchical read-only or execute-never bits set.
                */
               PG_SET_ENTRY_RAW(buffer, i, PTR_TO_UINT(next_buf) |
                                PG_CLEAN_TABLE_NOEXEC(
                                   PG_CLEAN_TABLE_READONLY(
                                      entry & pa_mask
                                      )));
            }
            valid_entries++;
         }

         table_count += traverse_count;
      }
   }

   if (valid_entries != 0) {
      return table_count;
   }

   return 0;
}

static EFI_PHYSICAL_ADDRESS page_table_base;
static size_t page_table_pages;
static uint64_t page_table_mask;

/*-- allocate_page_tables ------------------------------------------------------
 *
 *      Allocate enough bootloader memory for later use by relocate_page_tables.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int allocate_page_tables(void)
{
   UINT32 MMapVersion;
   uintptr_t pdbr = 0;
   EFI_STATUS Status;

   EFI_ASSERT(is_paging_enabled());

   /*
    * Save a copy of the page table mask as SEV-ES VMs will have problems if
    * they try to execute cpuid after ExitBootServices.
    */
   page_table_mask = get_page_table_mask();

   /*
    * Get the memory map. We need it for two reasons:
    * 1) Know what VA ranges correspond to real RAM, so we can
    *    properly sanitize the page tables when we copy them.
    * 2) To be able to type existing page table mappings. The memory
    *    map will let us figure out if a mapping is "conventional"
    *    used or free memory, and we'll treat everything else as MMIO
    *    (that is, never executable).
    *
    * Note: it doesn't matter that we use a "stale" version for typing
    * page table mappings, because we are not concerned with the actual
    * type of a range, but just whether it corresponds to a usable RAM type
    * or not. The whole point of these acrobatics is to ensure we do not
    * ever wind up mapping a reserved or MMIO physical range as executable,
    * as that can be catastrophic on Arm.
    */
   Status = efi_get_memory_map(0, &pt_reloc_cached_mmap,
                               &pt_reloc_cached_mmap_size,
                               &pt_reloc_cached_mmap_desc_size,
                               &MMapVersion);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   set_memtop();
   Log(LOG_DEBUG, "Measuring existing page tables...");

   // Figure how much space is needed to copy the page tables over.
   get_page_table_reg(&pdbr);
   pdbr &= ~0xfffULL;
   page_table_pages = traverse_page_tables_rec(UINT_TO_PTR(pdbr),
                                               PG_TABLE_MAX_LEVELS, 0,
                                               page_table_mask, 0,
                                               NULL, NULL);

   // Allocate this space in EfiLoaderData memory
   Log(LOG_DEBUG, "...allocating new page tables...");
   EFI_ASSERT_FIRMWARE(bs->AllocatePages != NULL);
   Status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData,
                              page_table_pages, &page_table_base);
   if (EFI_ERROR(Status)) {
      int status = error_efi_to_generic(Status);
      Log(LOG_ERR, "Error allocating %"PRIu64" pages: %s",
          page_table_pages, error_str[status]);
      return status;
   }

   Log(LOG_DEBUG, "...will move %"PRIu64" pages to 0x%"PRIx64"",
       page_table_pages, page_table_base);

   return ERR_SUCCESS;
}

/*-- relocate_page_tables1 -----------------------------------------------------
 *
 *      Temporarily relocate the memory page tables into the previously
 *      allocated EfiLoaderData memory, adding write and execute permissions in
 *      the process. Then reload the page table base pointer to point to the
 *      new page tables.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int relocate_page_tables1(void)
{
   uintptr_t pdbr = 0;
   uint64_t mask = page_table_mask;

   Log(LOG_DEBUG, "Copying page tables...");
   get_page_table_reg(&pdbr);
   pdbr &= ~0xfffULL;

   traverse_page_tables_rec(UINT_TO_PTR(pdbr), PG_TABLE_MAX_LEVELS,
                            0, mask, 0, (uint64_t *)page_table_base,
                            (uint64_t *)(page_table_base +
                                         page_table_pages * PAGE_SIZE));
   Log(LOG_DEBUG, "...switching page tables 1...");
   set_page_table_reg((uintptr_t *)&page_table_base);
   Log(LOG_DEBUG, "...running on new page tables");

   return ERR_SUCCESS;
}

/*-- relocate_page_tables2 -----------------------------------------------------
 *
 *      Relocate the memory page tables again, this time into safe memory, to
 *      be sure they are out of the way of the boot modules.  Then reload the
 *      page table base pointer to point to the new page tables.  Must be
 *      called only after module allocation and runtime memory blacklisting is
 *      complete.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int relocate_page_tables2(void)
{
   uintptr_t pdbr = 0;
   uint64_t mask = page_table_mask;
   int status;

   Log(LOG_DEBUG, "Relocating memory mapping tables again...");

   status = alloc(&page_table_base, page_table_pages * PAGE_SIZE,
                  ALIGN_PAGE, ALLOC_ANY);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Page tables relocation error: out of safe memory.");
      return status;
   }

   Log(LOG_DEBUG, "...moving %zd pages to %p",
       page_table_pages, UINT64_TO_PTR(page_table_base));

   Log(LOG_DEBUG, "Copying page tables...");
   get_page_table_reg(&pdbr);
   pdbr &= ~0xfffULL;

   traverse_page_tables_rec(UINT_TO_PTR(pdbr), PG_TABLE_MAX_LEVELS,
                            0, mask, 0, (uint64_t *)page_table_base,
                            (uint64_t *)(page_table_base +
                                         page_table_pages * PAGE_SIZE));
   Log(LOG_DEBUG, "...switching page tables 2...");
   set_page_table_reg((uintptr_t *)&page_table_base);
   Log(LOG_DEBUG, "...running on new page tables");

   return ERR_SUCCESS;
}
#endif /* defined(only_em64t) || defined(only_arm64) */

/*-- exit_boot_services --------------------------------------------------------
 *
 *      Exit UEFI boot services.
 *
 * Parameters
 *      IN  desc_extra_mem: amount of extra memory (in bytes) needed for each
 *                          memory map descriptor
 *      OUT mmap:           pointer to the E820 system memory map
 *      OUT count:          number of entry in the memory map
 *      OUT efi_info:       EFI information
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      Shutdown the boot services and invalidate the 'bs' global pointer.
 *      EFI Boot Services are no longer available after a call to this function.
 *----------------------------------------------------------------------------*/
int exit_boot_services(size_t desc_extra_mem, e820_range_t **mmap,
                       size_t *count, efi_info_t *efi_info)
{
   EFI_STATUS Status;
   int error;

   EFI_ASSERT(st != NULL);
   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->ExitBootServices != NULL);

   if ((efi_info->quirks & EFI_NET_DEV_DISABLE) != 0) {
      disable_network_controllers();
   }

#if defined(only_em64t) || defined(only_arm64)
   error = sanitize_page_tables();
   if (error != ERR_SUCCESS) {
      return error;
   }

   error = allocate_page_tables();
   if (error != ERR_SUCCESS) {
      return error;
   }
#endif /* defined(only_em64t) || defined(only_arm64) */

   /*
    * UEFI Specification v2.3 (6.4. "Image Services", ExitBootServices()) says:
    *
    * "An EFI OS loader must ensure that it has the system's current memory map
    *  at the time it calls ExitBootServices(). This is done by passing in the
    *  current memory map's MapKey value as returned by GetMemoryMap().
    *  Care must be taken to ensure that the memory map does not change between
    *  these two calls. It is suggested that GetMemoryMap() be called
    *  immediately before calling ExitBootServices()."
    */
   Log(LOG_DEBUG, "About to ExitBootServices...");
again:
   error = get_memory_map(desc_extra_mem, mmap, count, efi_info);
   if (error != ERR_SUCCESS) {
      return error;
   }

   Status = bs->ExitBootServices(ImageHandle, MapKey);
   if (Status == EFI_INVALID_PARAMETER) {
      free_memory_map(*mmap, efi_info);
      Log(LOG_DEBUG, "...must retry ExitBootServices...");
      goto again;
   }
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }
   Log(LOG_DEBUG, "...successful");

   /*
    * UEFI Specification v2.3 (6.4. "Image Services") says:
    *
    * "On ExitBootServices() success, several fields of the EFI System Table
    *  should be set to NULL. These include ConsoleInHandle, ConIn,
    *  ConsoleOutHandle, ConOut, StandardErrorHandle, StdErr, and
    *  BootServicesTable. In addition, since fields of the EFI System Table
    *  are being modified, the 32-bit CRC for the EFI System Table must be
    *  recomputed."
    */
   bs = NULL;
   st->ConsoleInHandle = NULL;
   st->ConIn = NULL;
   st->ConsoleOutHandle = NULL;
   st->ConOut = NULL;
   st->StandardErrorHandle = NULL;
   st->StdErr = NULL;
   st->BootServices = NULL;
   st->Hdr.CRC32 = 0;
   st->Hdr.CRC32 = crc_32(&st->Hdr, st->Hdr.HeaderSize);

   efi_info->systab = PTR_TO_UINT64(st);
   efi_info->systab_size = st->Hdr.HeaderSize;
   efi_info->valid = true;

#if defined(only_em64t) || defined(only_arm64)
   error = relocate_page_tables1();
   if (error != ERR_SUCCESS) {
      return error;
   }
#endif /* defined(only_em64t) || defined(only_arm64) */

   return ERR_SUCCESS;
}
