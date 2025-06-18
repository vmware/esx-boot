/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc.
 * and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * memory.c -- EFI-specific memory management functions
 */

#include <string.h>
#include <e820.h>
#include "efi_private.h"
#include "cpu.h"

UINTN MapKey;

static EFI_MEMORY_TYPE ImageDataType = EfiReservedMemoryType;

/*-- efi_get_memory_map --------------------------------------------------------
 *
 *      Get the EFI-specific memory map.
 *
 * Parameters
 *      IN  DescExtraMem: amount of extra memory (in bytes) needed for each
 *                        memory map entry, over and above SizeOfDesc
 *      OUT MemMap:       pointer to the EFI memory map
 *      OUT Size:         memory map size in bytes (not including the extra
 *                        memory)
 *      OUT SizeOfDesc:   actual size in bytes of a UEFI memory map entry (not
 *                        including the descriptors extra memory)
 *      OUT MMapVersion:  UEFI memory descriptor version
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *
 * Side Effects
 *      The MapKey global variable is set here and will be passed to
 *      ExitBootServices() to verify the memory map consistency when shutting
 *      down the boot services.
 *----------------------------------------------------------------------------*/
EFI_STATUS efi_get_memory_map(UINTN desc_extra_mem,
                              EFI_MEMORY_DESCRIPTOR **MMap,
                              UINTN *Size, UINTN *SizeOfDesc,
                              UINT32 *MMapVersion)
{
   EFI_MEMORY_DESCRIPTOR *Buffer = NULL;
   UINTN BufLen, DescSize;
   UINT32 Version;
   EFI_STATUS Status;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->GetMemoryMap != NULL);
   EFI_ASSERT_PARAM(MMap != NULL);
   EFI_ASSERT_PARAM(Size != NULL);
   EFI_ASSERT_PARAM(SizeOfDesc != NULL);

   do {
      if (Buffer != NULL) {
         free(Buffer);
      }
      BufLen = 0;
      Buffer = NULL;
      DescSize = 0;

      Status = bs->GetMemoryMap(&BufLen, NULL, &MapKey, &DescSize, &Version);
      if (Status != EFI_BUFFER_TOO_SMALL) {
         return Status;
      }

      /*
       * At this point we know the desired size, but are not guaranteed
       * DescSize. The specification says nothing about returning
       * a valid DescSize on EFI_BUFFER_TOO_SMALL, and U-Boot doesn't.
       *
       * What's even more exciting, is that the spec tells you to allocate
       * more than the reported memory map size, but provides no guidance
       * on what "more" is supposed to constitute (remember, you don't
       * know the descriptor size, and have no idea about how many
       * separate new descriptors could be created due to the
       * malloc below). So just double it.
       */
      BufLen *= 2;

      Buffer = malloc(BufLen);
      if (Buffer == NULL) {
         return EFI_OUT_OF_RESOURCES;
      }
      Status = bs->GetMemoryMap(&BufLen, Buffer, &MapKey, &DescSize, &Version);
      if (Status != EFI_SUCCESS) {
         /*
          * If we got EFI_BUFFER_TOO_SMALL, retry else fail on everything else.
          *
          * Yes, this is possible, if allocation happened in a DPC somewhere
          * between the two GetMemoryMap calls (e.g. bus enumeration, but could
          * be anything).
          */
         continue;
      }

      /*
       * Now we know DescSize and can allocate Buffer for real.
       */
      EFI_ASSERT_FIRMWARE(DescSize > 0);
      free(Buffer);
      /*
       * We know DescSize, but it's unclear by how many descriptors the
       * memory map could grow as a result of the allocation below.
       * You'd hope that it can at most result in splitting an entry
       * into two entries, but this is implementation specific - what if someone
       * decided to create guard allocations? Again, just double it.
       */
      BufLen *= 2;
      /*
       * Now adjust by desc_extra_mem.
       */
      BufLen += (BufLen / DescSize) * desc_extra_mem;
      Buffer = malloc(BufLen);
      if (Buffer == NULL) {
         return EFI_OUT_OF_RESOURCES;
      }

      Status = bs->GetMemoryMap(&BufLen, Buffer, &MapKey, &DescSize, &Version);
      if (!EFI_ERROR(Status)) {
         *MMap = Buffer;
         *Size = BufLen;
         *SizeOfDesc = DescSize;
         *MMapVersion = Version;
         return EFI_SUCCESS;
      }
   } while (Status == EFI_BUFFER_TOO_SMALL);

   free(Buffer);

   return Status;
}

/*-- get_memory_map ------------------------------------------------------------
 *
 *      Get the system memory map as returned by the E820 BIOS-call, and the
 *      raw EFI memory map. In addition to the standard E820 memory types, an
 *      other value is defined for locating the bootloader memory.
 *
 *      E820_TYPE_BOOTLOADER memory is for the bootloader's internal usage and
 *      should never be known by the kernel which always considers such memory
 *      as available. Therefore, it is the bootloader's responsibility to
 *      convert any E820_TYPE_BOOTLOADER entry to the E820_TYPE_AVAILABLE type
 *      before passing the system memory map to the kernel.
 *
 *   The 'desc_extra_mem' parameter
 *      Depending on the dynamic memory allocator implementation, the system
 *      memory map may vary after each call to malloc(), sys_realloc(), or
 *      free(). This raises the following problem:
 *
 *      Let's consider a situation where we need to convert the E820 memory map
 *      to a different format (e.g. the Multiboot format). Both memory maps
 *      would contain the same number of entries, but the Multiboot memory map
 *      would be made of bigger descriptors. Obviously, we would need to
 *      allocate additional space in order to process the conversion.
 *
 *      It is a tricky case, because allocating memory modifies the memory map,
 *      so it is not possible to allocate memory after getting the memory map.
 *      At the same time, the amount of memory to allocate directly depends of
 *      the number of descriptors in the memory map. Then it cannot be allocated
 *      before getting the memory map.
 *
 *      To solve this issue, the desc_extra_mem parameter is used to specify
 *      the amount of extra memory, on top of the size of an e820_range_t, that
 *      is needed for each descriptor in the E820 map. The raw EFI memory map
 *      is not affected by this parameter.
 *
 *      Warning: Details of freeing the map vary between BIOS and EFI
 *      implementations.  Use free_memory_map if the map needs to be freed.
 *
 * Parameters
 *      IN  desc_extra_mem: extra size needed for each entry (in bytes)
 *      OUT e820_mmap:      pointer to the memory map (not sorted, not merged)
 *      OUT count:          number of descriptors in the memory map
 *      OUT efi_info:       EFI memory map information is filled in
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_memory_map(size_t desc_extra_mem, e820_range_t **e820_mmap,
                   size_t *count, efi_info_t *efi_info)
{
   EFI_MEMORY_DESCRIPTOR *MMap;
   e820_range_t *mmap;
   UINTN i, nEntries, MMapSize, SizeOfDesc;
   uint64_t base, length;
   UINT32 MMapVersion;
   uint32_t type;
   EFI_STATUS Status;

   SizeOfDesc = 0;
   MMapSize = 0;
   MMap = NULL;
   MMapVersion = 0;

   Status = efi_get_memory_map(desc_extra_mem + sizeof (e820_range_t),
                               &MMap, &MMapSize, &SizeOfDesc, &MMapVersion);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   nEntries = MMapSize / SizeOfDesc;
   mmap = (e820_range_t *)((char*)MMap + MMapSize);

   if (efi_info->mmap != NULL) {
      EFI_ASSERT(efi_info->num_descs != 0);
      EFI_ASSERT(efi_info->desc_size != 0);
      free(efi_info->mmap);
      efi_info->mmap = NULL;
      efi_info->num_descs = 0;
      efi_info->desc_size = 0;
      efi_info->version = 0;
   }
   efi_info->mmap = MMap;
   efi_info->num_descs = nEntries;
   efi_info->desc_size = SizeOfDesc;
   efi_info->version = MMapVersion;

   for (i = 0; i < nEntries; i++) {
      base = MMap->PhysicalStart;
      length = MMap->NumberOfPages << EFI_PAGE_SHIFT;

      switch (MMap->Type) {
         case EfiLoaderData:
         case EfiLoaderCode:
            type = E820_TYPE_BOOTLOADER;
            break;
         case EfiBootServicesCode:
            type = E820_TYPE_AVAILABLE;
            break;
         case EfiBootServicesData:
            /*
             * Between ExitBootServices and entry to the trampoline, we are
             * running on a stack in EfiBootServicesData memory.  (GDT and IDT
             * are likely also there, though that doesn't matter so much.)  So
             * we blacklist such memory along with EfiLoaderCode and Data, to
             * prevent alloc() from handing it out for immediate use while we
             * are still running in C code. PR 2162215 update #28.
             */
            type = E820_TYPE_BOOTLOADER;
            break;
         case EfiConventionalMemory:
            if ((MMap->Attribute & EFI_MEMORY_NV) == 0) {
               type = E820_TYPE_AVAILABLE;
            } else {
               type = E820_TYPE_PMEM;
            }
            break;
         case EfiPersistentMemory:
            type = E820_TYPE_PMEM;
            break;
         case EfiACPIReclaimMemory:
            type = E820_TYPE_ACPI;
            break;
         case EfiACPIMemoryNVS:
            type = E820_TYPE_ACPI_NVS;
            break;
         case EfiRuntimeServicesCode:
            type = (efi_info->rts_vaddr > 0) ?
               E820_TYPE_RTS_CODE : E820_TYPE_RESERVED;
            break;
         case EfiRuntimeServicesData:
            type = (efi_info->rts_vaddr > 0) ?
               E820_TYPE_RTS_DATA : E820_TYPE_RESERVED;
            break;
         case EfiMemoryMappedIO:
            type = (efi_info->rts_vaddr > 0) ?
               E820_TYPE_RTS_MMIO : E820_TYPE_RESERVED;
            break;
         default:
            type = E820_TYPE_RESERVED;
            break;
      }

      e820_set_entry(&mmap[i], base, length, type, E820_ATTR_ENABLED);

      MMap = NextMemoryDescriptor(MMap, SizeOfDesc);
   }

   *e820_mmap = mmap;
   *count = nEntries;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- efi_log_memory_map --------------------------------------------------------
 *
 *      Log UEFI system memory map currently saved in efi_info.
 *
 * Parameters
 *      IN  efi_info: efi_info_t
 *----------------------------------------------------------------------------*/
void efi_log_memory_map(efi_info_t *efi_info)
{
   UINTN i;
   EFI_MEMORY_DESCRIPTOR *MMap;

   MMap = efi_info->mmap;
   EFI_ASSERT(MMap != NULL);

   for (i = 0; i < efi_info->num_descs; i++) {
      uint64_t base, length;
      base = MMap->PhysicalStart;
      length = MMap->NumberOfPages << EFI_PAGE_SHIFT;

      Log(LOG_INFO, "MMap[%zu]: "
          "0x%"PRIx64" - 0x%"PRIx64" len=%"PRIu64", type=%u, attr=0x%"PRIx64"",
          i, base, base + length - 1, length, MMap->Type, MMap->Attribute);

      MMap = NextMemoryDescriptor(MMap, efi_info->desc_size);
   }
}

/*-- log_memory_map -----------------------------------------------------------
 *
 *      Log system memory map.
 *
 *      Warning: Details of logging the map vary between BIOS and EFI
 *      implementations.  Use this function if the map needs to be logged.
 *
 *      The UEFI implementation logs the UEFI memory map, not the
 *      generated e820 one.
 *
 * Parameters
 *      IN  efi_info: efi_info_t
 *----------------------------------------------------------------------------*/
void log_memory_map(efi_info_t *efi_info)
{
   e820_range_t *e820_mmap;
   size_t count;

   if (get_memory_map(0, &e820_mmap, &count, efi_info) != ERR_SUCCESS) {
      Log(LOG_ERR, "failed to get memory map for logging");
      return;
   }

   efi_log_memory_map(efi_info);

   free_memory_map(e820_mmap, efi_info);
}

/*-- free_memory_map -----------------------------------------------------------
 *
 *      Free the system memory map allocated by get_memory_map.
 *
 *      Warning: Details of freeing the map vary between BIOS and EFI
 *      implementations.  Use this function if the map needs to be freed.
 *
 * Parameters
 *      IN  e820_map: e820 map
 *      IN  efi_info: efi_info_t containing pointer to efi map
 *----------------------------------------------------------------------------*/
void free_memory_map(UNUSED_PARAM(e820_range_t *e820_mmap),
                     efi_info_t *efi_info)
{
   free(efi_info->mmap);
   efi_info->mmap = NULL;
}

/*-- malloc --------------------------------------------------------------------
 *
 *      Allocate dynamic memory.
 *
 * Parameters
 *      IN size: amount of contiguous memory to allocate
 *
 * Results
 *      A pointer to the allocated memory, or NULL if an error occurred.
 *----------------------------------------------------------------------------*/
VOID *malloc(UINTN size)
{
   EFI_STATUS Status;
   VOID *p;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->AllocatePool != NULL);
   EFI_ASSERT(ImageDataType < EfiMaxMemoryType);

   Status = bs->AllocatePool(ImageDataType, size, &p);

   return EFI_ERROR(Status) ? NULL : p;
}

/*-- sys_realloc ---------------------------------------------------------------
 *
 *      Adjust the size of a previously allocated buffer.  Unlike standard
 *      realloc, this function requires the old size as a parameter.
 *
 * Parameters
 *      IN ptr:     pointer to the old memory buffer
 *      IN oldsize: size of the old memory buffer
 *      IN newsize: new desired size
 *
 * Results
 *      A pointer to the allocated memory, or NULL if an error occurred.
 *----------------------------------------------------------------------------*/
VOID *sys_realloc(VOID *ptr, UINTN oldsize, UINTN newsize)
{
   VOID *p = NULL;

   if (newsize > 0) {
      p = malloc(newsize);
   }

   if (ptr != NULL && p != NULL) {
      memcpy(p, ptr, MIN(oldsize, newsize));
      free(ptr); // like standard realloc, free only on success
   }

   return p;
}

/*-- free ----------------------------------------------------------------------
 *
 *      Free the memory space pointed to by 'ptr', which must have been returned
 *      by a previous call to malloc(). If 'ptr' is NULL, no operation is
 *      performed.
 *
 * Parameters
 *      IN ptr: pointer to the memory to free
 *----------------------------------------------------------------------------*/
VOID free(VOID *ptr)
{
   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->FreePool != NULL);
   EFI_ASSERT(ImageDataType < EfiMaxMemoryType);

   if (ptr != NULL) {
      bs->FreePool(ptr);
   }
}

/*-- mem_init ------------------------------------------------------------------
 *
 *      Initialize the dynamic memory allocator.
 *
 * Parameters
 *      IN MemType: type of memory that the allocator will provide
 *----------------------------------------------------------------------------*/
void mem_init(EFI_MEMORY_TYPE MemType)
{
   EFI_ASSERT(MemType < EfiMaxMemoryType);

   ImageDataType = MemType;
}

/*-- blacklist_specific_purpose_memory -----------------------------------------
 *
 *      Blacklist all SPM(Specific Purpose Memory) ranges so that bootloader
 *      won't use it for relocation.
 *
 *      Note: Called after exit boot services, so be careful to not try
 *      allocating any memory.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int blacklist_specific_purpose_memory(efi_info_t *efi_info)
{
   EFI_MEMORY_DESCRIPTOR *MMap;
   uint64_t base, length;
   UINTN i;
   int status = ERR_SUCCESS;

   MMap = efi_info->mmap;
   for (i = 0; i < efi_info->num_descs; i++) {
      base = MMap->PhysicalStart;
      length = MMap->NumberOfPages << EFI_PAGE_SHIFT;

      switch (MMap->Type) {
         case EfiConventionalMemory:
         case EfiPersistentMemory:
            if ((MMap->Attribute & EFI_MEMORY_SP) != 0) {
               status = blacklist_runtime_mem(base, length);
               if (status != ERR_SUCCESS) {
                  return status;
               }
            }
            break;
      }
      MMap = NextMemoryDescriptor(MMap, efi_info->desc_size);
   }
   return status;
}
