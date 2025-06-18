/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc.
 * and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * memory.c -- Memory management functions
 */

#include <string.h>
#include <stdlib.h>
#include <e820.h>

#include "com32_private.h"

#define E820_SIGNATURE          (('S' << 24) + ('M' << 16) + ('A' << 8) + 'P')
#define E820_MIN_SIZEOF_DESC    20

/*-- int12_get_memory_size -----------------------------------------------------
 *
 *      Get the amount of available conventional memory starting at address 0x0.
 *
 * Parameters
 *      OUT lowmem_size: amount of memory, in bytes.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int int12_get_memory_size(size_t *lowmem)
{
   com32sys_t oregs;
   uint16_t mem_size;

   intcall(0x12, NULL, &oregs);

   mem_size = oregs.eax.w[0];

   if (mem_size < 32 || mem_size > 640) {
      return ERR_UNSUPPORTED;
   }

   *lowmem = (size_t)mem_size << 10;

   return ERR_SUCCESS;
}

/*-- int15_e801 ----------------------------------------------------------------
 *
 *      Get the amount of available extended memory.
 *
 * Parameters
 *      OUT s1: configured memory 1M to 16M, in bytes
 *      OUT s2: configured memory above 16M, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int int15_e801(size_t *s1, size_t *s2)
{
   com32sys_t iregs, oregs;
   int status;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0xe801;
   status = intcall_check_CF(0x15, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *s1 = (size_t)oregs.ecx.w[0] << 10;
   *s2 = (size_t)oregs.edx.w[0] << 16;

   return ERR_SUCCESS;
}

/*-- int15_88 ------------------------------------------------------------------
 *
 *      Get the amount of available extended memory.
 *
 * Parameters
 *      OUT size: extended memory above 1MB, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int int15_88(size_t *size)
{
   com32sys_t iregs, oregs;
   uint16_t mem_size;
   int status;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.b[1] = 0x88;
   status = intcall_check_CF(0x15, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   mem_size = oregs.eax.w[0];
   if (size == 0 || oregs.eax.b[1] == 0x80 || oregs.eax.b[1] == 0x86) {
      return ERR_UNSUPPORTED;
   }

   *size = (size_t)mem_size << 10;

   return ERR_SUCCESS;
}

/*-- int15_e820 ----------------------------------------------------------------
 *
 *      Interrupt 15h wrapper to get a E820 memory map descriptor.
 *
 * Parameters
 *      IN  desc:      pointer to the memory map descriptor buffer
 *      IN  next:      continuation offset or 0 to start at beginning of map
 *      OUT next:      next offset from which to copy or 0 if all done
 *      OUT desc_size: actual descriptor size in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int int15_e820(e820_range_t *desc, uint32_t *next, uint32_t *desc_size)
{
   com32sys_t iregs, oregs;
   e820_range_t *buf;
   farptr_t fptr;
   size_t size;
   int status;

   buf = get_bounce_buffer();
   memset(buf, 0, sizeof (e820_range_t));

   /*
    * Just in case the BIOS would only write the first 20 bytes of the
    * descriptor and would fail to update the descriptor size.
    */
   buf->attributes |= E820_ATTR_ENABLED;

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(buf);
   iregs.eax.w[0] = 0xe820;
   iregs.ebx.l = *next;
   iregs.ecx.l = sizeof (e820_range_t);
   iregs.edx.l = E820_SIGNATURE;
   iregs.edi.w[0] = fptr.real.offset;
   iregs.es = fptr.real.segment;

   status = intcall_check_CF(0x15, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (oregs.eax.l != E820_SIGNATURE) {
      return ERR_INCONSISTENT_DATA;
   }

   size = oregs.ecx.l;
   if (size < E820_MIN_SIZEOF_DESC) {
      return ERR_BAD_BUFFER_SIZE;
   }

   if (size > sizeof (e820_range_t)) {
      return ERR_BUFFER_TOO_SMALL;
   }

   memcpy(desc, buf, size);
   *desc_size = size;
   *next = oregs.ebx.l;

   return ERR_SUCCESS;
}

/*-- sanitize_e820_mmap --------------------------------------------------------
 *
 *      ACPI v4.0 claims that bit-0 of a memory map descriptor extended
 *      attributes is reserved and must be set to 1. ACPI v3.0b even commands to
 *      ignore the descriptors which have this bit cleared.
 *
 *      Unfortunately, some platforms [*] never set this bit, and we would end
 *      up ignoring the whole memory map on these platforms if we were as strict
 *      as ACPI v3.0b.
 *
 *      [*] The Dell PowerEdge R710 and Dell Poweredge T610 are known for having
 *          this issue with firmware version 1.03.10. Note that firmware 2.2.10
 *          fixes it.
 *
 *      To work around this BIOS bug, we first look at the entire memory map.
 *
 *        - If all entries in the memory map have bit-0 cleared in their
 *          extended attributes, then we treat them as if they were all valid
 *          (setting bit-0).
 *
 *        - if only a few entries have bit-0 cleared in their extended
 *          attributes, we remove these entries from the memory map.
 *
 * Parameters
 *      IN  mmap:  pointer to the e820 memory map
 *      IN  count: number of entries in the memory map
 *      OUT count: updated number of entries in the memory map
 *----------------------------------------------------------------------------*/
static void sanitize_e820_mmap(e820_range_t *mmap, size_t *count)
{
   bool use_extended_attributes;
   size_t i, nentries;

   nentries = *count;
   if (nentries == 0) {
      return;
   }

   use_extended_attributes = false;

   for (i = 0; i < nentries; i++) {
      if (mmap[i].attributes & E820_ATTR_ENABLED) {
         use_extended_attributes = true;
      }
   }

   if (!use_extended_attributes) {
      for (i = 0; i < nentries; i++) {
         mmap[i].attributes |= E820_ATTR_ENABLED;
      }

      return;
   }

   i = 0;
   while (i < nentries) {
      if (mmap[i].attributes & E820_ATTR_ENABLED) {
         i++;
      } else {
         if (i != nentries - 1) {
            memcpy(&mmap[i], &mmap[nentries - 1], sizeof (e820_range_t));
         }
         nentries--;
      }
   }

   *count = nentries;
}

/*-- get_e820_mmap -------------------------------------------------------------
 *
 *      Get the E820 memory map from the BIOS.
 *
 * Parameters
 *      OUT mmap:  pointer to the memory map (not sorted, not merged)
 *      OUT count: number of descriptors in the memory map
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_e820_mmap(e820_range_t **mmap, size_t *count)
{
   size_t nentries, max_nentries;
   e820_range_t *e820, *p;
   uint32_t desc_size, next;
   uint64_t base, len;
   int status;

   next = 0;
   nentries = 0;
   max_nentries = 0;
   e820 = NULL;

   do {
      if (nentries >= max_nentries) {
         max_nentries += 64;
         p = realloc(e820, max_nentries * sizeof (e820_range_t));
         if (p == NULL) {
            free(e820);
            return ERR_OUT_OF_RESOURCES;
         }
         e820 = p;
      }

      status = int15_e820(&e820[nentries], &next, &desc_size);
      if (status != ERR_SUCCESS) {
         free(e820);
         return status;
      }

      base = E820_BASE(&e820[nentries]);
      len = E820_LENGTH(&e820[nentries]);

      if (desc_size < sizeof (e820_range_t)) {
         Log(LOG_DEBUG, "e820[%d]: 0x%llx - 0x%llx "
             " len=%llu, type=%u, no attr\n",
             nentries, base, base + len - 1, len, e820[nentries].type);

         e820[nentries].attributes = E820_ATTR_ENABLED;
      } else {
         Log(LOG_DEBUG,
             "e820[%d]: 0x%llx - 0x%llx len=%llu, type=%u, attr=0x%x%s\n",
             nentries, base, base + len - 1, len, e820[nentries].type,
             e820[nentries].attributes,
             (e820[nentries].attributes & E820_ATTR_ENABLED) ?
             "" : " INVALID");
      }

      nentries++;
   } while (next != 0);

   sanitize_e820_mmap(e820, &nentries);
   if (nentries == 0) {
      free(e820);
      return ERR_NOT_FOUND;
   }

   *mmap = e820;
   *count = nentries;

   return ERR_SUCCESS;
}

/*-- get_memory_map ------------------------------------------------------------
 *
 *      Get the system memory map. This function assumes that the BIOS is recent
 *      enough to support the INT15h E820 interrupt call, and does not bother
 *      with the legacy E801h/88h interrupt calls.
 *
 *      Warning: Details of freeing the map vary between BIOS and EFI
 *      implementations.  Use free_memory_map if the map needs to be freed.
 *
 * Parameters
 *      IN  desc_extra_mem: extra size needed for each entry (in bytes)
 *      OUT mmap:           pointer to the memory map (not sorted, not merged)
 *      OUT count:          number of descriptors in the memory map
 *      OUT efi_info:       unused
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_memory_map(size_t desc_extra_mem, e820_range_t **mmap, size_t *count,
                   UNUSED_PARAM(efi_info_t *efi_info))
{
   e820_range_t *e820 = NULL;
   e820_range_t *tmp;
   size_t nentries = 0;
   int status;

   status = get_e820_mmap(&e820, &nentries);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (desc_extra_mem > 0) {
      tmp = e820;
      e820 = realloc(tmp, nentries * (sizeof (e820_range_t) + desc_extra_mem));
      if (e820 == NULL) {
         free(tmp);
         return ERR_OUT_OF_RESOURCES;
      }
   }

   *mmap = e820;
   *count = nentries;

   return ERR_SUCCESS;
}

/*-- log_memory_map -----------------------------------------------------------
 *
 *      Log system memory map.
 *
 *      Warning: Details of logging the map vary between BIOS and EFI
 *      implementations.  Use this function if the map needs to be logged.
 *
 * Parameters
 *      IN  efi_info: efi_info_t
 *----------------------------------------------------------------------------*/
void log_memory_map(UNUSED_PARAM(efi_info_t *efi_info))
{
   e820_range_t *e820_mmap;
   size_t count;

   /*
    * In the legacy BIOS case, logging is a side effect of getting the map.
    */
   if (get_memory_map(0, &e820_mmap, &count, NULL) == ERR_SUCCESS) {
      free_memory_map(e820_mmap, NULL);
      log_malloc_arena();
   } else {
      Log(LOG_ERR, "failed to get memory map for logging\n");
   }
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
void free_memory_map(e820_range_t *e820_mmap,
                     UNUSED_PARAM(efi_info_t *efi_info))
{
   free(e820_mmap);
}

/*-- sys_realloc ---------------------------------------------------------------
 *
 *      Generic wrapper for the COM32 realloc().  Unlike standard realloc, this
 *      interface takes the old size as a parameter so as to be usable in both
 *      COM32 and UEFI builds.
 *
 * Parameters
 *      IN ptr:     pointer to the old memory buffer
 *      IN oldsize: size of the old memory buffer, in bytes
 *      IN newsize: new desired size, in bytes
 *
 * Results
 *      A pointer to the allocated memory, or NULL if an error occurred.
 *----------------------------------------------------------------------------*/
void *sys_realloc(void *ptr, UNUSED_PARAM(size_t oldsize), size_t newsize)
{
   return realloc(ptr, newsize);
}

/*-- blacklist_specific_purpose_memory -----------------------------------------
 *
 *    on UEFI systems used to black list SPM(Specific Purpose Memrory),
 *    on BIOS its just a No-OP.
 *
 * Results
 *      ERR_SUCCESS
 *----------------------------------------------------------------------------*/
int blacklist_specific_purpose_memory(UNUSED_PARAM(efi_info_t *efi_info))
{
   return ERR_SUCCESS;
}
