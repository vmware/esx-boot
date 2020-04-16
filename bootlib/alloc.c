/*******************************************************************************
 * Copyright (c) 2008-2011,2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * alloc.c -- Memory allocator
 *
 *   Simple memory allocator used for run-time memory, that is, memory for
 *   relocating into. The allocator only keeps track of the allocated memory.
 *   Freeing memory is not supported.
 *
 *   The following macros are defined to facilitate the allocator usage:
 *
 *      runtime_alloc_fixed(addr, size)
 *         Allocates size bytes of memory at a fixed address.
 *
 *      runtime_alloc(addr, size, align)
 *         Allocates size bytes of memory at any aligned address.
 *
 *      blacklist_runtime_mem(addr, size)
 *         Reports a memory area as not available for allocation.
 *
 *   Note: In general, the memory may be allocated for *later* use, not
 *   immediate use.  Allocations prior to the point where
 *   blacklist_bootloader_mem is called return memory that is not safe to write
 *   into except by the trampoline.  Subsequent calls return memory that can be
 *   used immediately.
 */

#include <string.h>
#include <e820.h>
#include <bootlib.h>

#define MAX_ALLOCS_NR   4096

typedef struct {
   uint64_t base;
   uint64_t len;
} addr_range_t;

/*
 * Ideally, we would have the allocation table scalable dynamically. However,
 * boot services have been already shut down at the time we are dealing with
 * the allocator (so we don't have sys_malloc() anymore).
 */
static addr_range_t allocs[MAX_ALLOCS_NR];   /* The table of allocations */
static int alloc_count = 0;                  /* Number of allocations */

void alloc_sanity_check(void)
{
   uint64_t base, len, limit, max_limit;
   bool error;
   const char *msg;
   int i;

   if (alloc_count < 1) {
      Log(LOG_ERR, "Allocation table is empty.\n");
      while (1);
   }

   Log(LOG_DEBUG, "Allocation table count=%d, max=%d",
       alloc_count, MAX_ALLOCS_NR);

   error = false;
   max_limit = 0;

   for (i = 0; i < alloc_count; i++) {
      msg = NULL;
      base = allocs[i].base;
      len = allocs[i].len;
      limit = base + len - 1;

      if (len == 0) {
         msg = "zero-length allocation";
      } else if (!(i + 1 == alloc_count && base + len == 0) && base + len <= base) {
         msg = "Allocation range overflow";
      } else if ((i > 0 && base <= max_limit) || limit < max_limit) {
         msg = "Allocation table is not sorted";
      }

      if (msg != NULL) {
         error = true;
         Log(LOG_ERR, "%"PRIx64" - %"PRIx64" (%"PRIu64" bytes): %s.\n",
             base, limit, len, msg);
      }

      max_limit = limit;
   }

   if (error) {
      for (i = 0; i < alloc_count; i++) {
         base = allocs[i].base;
         len = allocs[i].len;
         limit = base + len - 1;
         Log(LOG_DEBUG, "%"PRIx64" - %"PRIx64" (%"PRIu64")\n",
             base, limit, len);
      }

      Log(LOG_ERR, "Allocation table is corrupted.\n");
      while (1);
   }
}

/*-- alloc_insert ---------------------------------------------------------
 *
 *      Insert a new entry in the allocation table.
 *
 * Parameters
 *      IN base:  memory range start address
 *      IN len:   memory range size
 *      IN index: the entry must be inserted at this position
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int alloc_insert(uint64_t base, uint64_t len, int index)
{
   if (alloc_count >= MAX_ALLOCS_NR) {
      Log(LOG_ERR, "Allocation table is full.\n");
      return ERR_OUT_OF_RESOURCES;
   }

   if (index > alloc_count) {
      return ERR_INVALID_PARAMETER;
   } else if (index < alloc_count) {
      memmove(&allocs[index + 1], &allocs[index],
              (alloc_count - index) * sizeof (addr_range_t));
   }

   allocs[index].base = base;
   allocs[index].len = len;
   alloc_count++;

   return ERR_SUCCESS;
}

/*-- alloc_add ------------------------------------------------------------
 *
 *      Add a memory range in the allocation table. This function keeps the
 *      memory ranges sorted by increasing base addresses.
 *      Overlapping memory ranges are merged.
 *
 * Parameters
 *      IN base: memory range start address
 *      IN len:  memory range size
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int alloc_add(uint64_t base, uint64_t len)
{
   uint64_t newbase, newlen;
   int n, idx, merges;

   idx = 0;
   for (n = 0; n < alloc_count; n++) {
      if ((base <= allocs[n].base) ||
          is_mergeable(base, len, allocs[n].base, allocs[n].len)) {
         break;
      }
      idx++;
   }

   merges = 0;
   for ( ; n < alloc_count; n++) {
      if (!is_mergeable(base, len, allocs[n].base, allocs[n].len)) {
         break;
      }
      merges++;
   }

   if (merges == 0) {
      /* No possible merges, just insert the allocation */
      return alloc_insert(base, len, idx);
   }

   newbase = MIN(base, allocs[idx].base);
   if (((base + len) < base) ||
       ((allocs[idx + merges - 1].base + allocs[idx + merges - 1].len)
        < allocs[idx + merges - 1].base)) {
      /* Avoids overflows */
      newlen = 0;
   } else {
      newlen = MAX(base + len, allocs[idx + merges - 1].base +
                   allocs[idx + merges - 1].len);
   }
   newlen -= newbase;

   if (merges > 1) {
      if (idx + merges < alloc_count) {
         memmove(&allocs[idx + 1], &allocs[idx + merges],
                 (alloc_count - (idx + merges)) * sizeof (addr_range_t));
      }
      alloc_count -= merges - 1;
   }

   allocs[idx].base = newbase;
   allocs[idx].len = newlen;

   return ERR_SUCCESS;
}

/*-- is_free_mem ---------------------------------------------------------------
 *
 *      Check whether a memory range is free.
 *
 * Parameters
 *      IN base: memory range start address
 *      IN len:  memory range size
 *
 * Results
 *      true if the memory is free, false otherwise.
 *----------------------------------------------------------------------------*/
static bool is_free_mem(uint64_t base, uint64_t len)
{
   int i;

   for (i = 0; i < alloc_count; i++) {
      if (is_overlap(base, len, allocs[i].base, allocs[i].len)) {
         return false;
      }
   }

   return true;
}

/*-- find_free_mem -------------------------------------------------------------
 *
 *      Find memory that has not been allocated yet. This function does not
 *      allocate the returned memory.
 *
 * Parameters
 *      IN  size:   amount of needed memory
 *      IN  align:  memory will have to be aligned on this much
 *      IN  option: ALLOC_32BIT or ALLOC_ANY
 *      OUT addr:   the aligned address of the found free memory
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int find_free_mem(uint64_t size, size_t align,
                         int option, uint64_t *addr)
{
   uint64_t hole_base, hole_len;
   uint64_t aligned_addr, padding;
   int i;

   hole_base = 0;

   for (i = 0; i < alloc_count; i++) {
      hole_len = allocs[i].base - hole_base;

      if (hole_len >= size) {
         aligned_addr = roundup64(hole_base, align);
         if (aligned_addr < hole_base) {
            // Overflow
            break;
         }

         padding = aligned_addr - hole_base;
         if (size + padding < size) {
            // Overflow
            break;
         }

         if (option == ALLOC_32BIT &&
             aligned_addr + size > MAX_32_BIT_ADDR) {
            break;
         }

         if (padding + size <= hole_len) {
            *addr = aligned_addr;
            return ERR_SUCCESS;
         }
      }

      hole_base = allocs[i].base + allocs[i].len;
   }

   return ERR_OUT_OF_RESOURCES;
}

/*-- alloc ---------------------------------------------------------------------
 *
 *      Allocate memory. If size is 0, then alloc() returns 0 and returned
 *      address is 0.
 *
 *      ALLOC_32BIT:  Allocate memory anywhere below 4GB.
 *      ALLOC_FIXED:  Allocate memory at a fixed address
 *      ALLOC_FORCE:  Same as ALLOC_FIXED, but do not fail if requested memory
 *                    had already been allocated. This is used at initialization
 *                    time, to manually reserve memory regions that we don't
 *                    want the allocator to return later.
 *      ALLOC_ANY:    Allocate memory anywhere, including above 4GB.
 *
 * Parameters
 *      IN  addr:     fixed address, in case of ALLOC_FIXED or ALLOC_FORCE
 *      OUT addr:     the address of the allocated memory
 *      IN  size:     amount of need memory
 *      IN  align:    returned address must be aligned on this much
 *      IN  option:   ALLOC_32BIT, ALLOC_FIXED, ALLOC_FORCE, or ALLOC_ANY
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int alloc(uint64_t *addr, uint64_t size, size_t align, int option)
{
   uint64_t base;
   int status;

   base = 0;

   if (size > 0) {
      if (option == ALLOC_FIXED && !is_free_mem(*addr, size)) {
         return ERR_OUT_OF_RESOURCES;
      }

      if (option == ALLOC_FIXED || option == ALLOC_FORCE) {
         base = *addr;
      } else {
         status = find_free_mem(size, align, option, &base);
         if (status != ERR_SUCCESS) {
            return status;
         }
      }

      status = alloc_add(base, size);
      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   *addr = base;

   return ERR_SUCCESS;
}
