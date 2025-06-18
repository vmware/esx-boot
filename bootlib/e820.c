/*******************************************************************************
 * Copyright (c) 2008-2016 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * e820.c -- E820 memory map management functions
 */

#include <string.h>
#include <e820.h>
#include <bootlib.h>

#define IS_E820_MERGEABLE(r1, r2)                                       \
   ((r1)->type == (r2)->type &&                                         \
    (r1)->attributes == (r2)->attributes &&                             \
    is_mergeable(E820_BASE(r1), E820_LENGTH(r1),                        \
                 E820_BASE(r2), E820_LENGTH(r2)))

/*-- is_mergeable --------------------------------------------------------------
 *
 *      Check whether two integer ranges can be merged. Two ranges can be merged
 *      if they overlap, or if they are contiguous.
 *
 * Parameters
 *      IN a1: first range start
 *      IN l1: first range length
 *      IN a2: second range start
 *      IN l2: second range length
 *
 * Results
 *      true/false
 *----------------------------------------------------------------------------*/
bool is_mergeable(uint64_t a1, uint64_t l1, uint64_t a2, uint64_t l2)
{
   uint64_t tmp;

   if (a2 < a1) {
      tmp = a1;
      a1 = a2;
      a2 = tmp;
      l1 = l2;
   }

   if (a1 + l1 < a1) {
      return true;
   }

   return a2 <= a1 + l1;
}

/*-- is_overlap ----------------------------------------------------------------
 *
 *      Check whether two integer ranges overlap.
 *
 * Parameters
 *      IN a1: first range start
 *      IN l1: first range length
 *      IN a2: second range start
 *      IN l2: second range length
 *
 * Results
 *      true/false
 *----------------------------------------------------------------------------*/
bool is_overlap(uint64_t a1, uint64_t l1, uint64_t a2, uint64_t l2)
{
   uint64_t tmp;

   if (a2 < a1) {
      tmp = a1;
      a1 = a2;
      a2 = tmp;
      l1 = l2;
   }

   if (a1 + l1 < a1) {
      return true;
   }

   return a2 < a1 + l1;
}

/*-- e820_compare --------------------------------------------------------------
 *
 *      For sorting the memory map descriptors by increasing base addresses.
 *
 * Parameters
 *      IN a: pointer to a memory map descriptor
 *      IN b: pointer to another memory map descriptor
 *
 * Results
 *      -1 when a < b
 *       1 when a > b
 *       0 when a == b
 *----------------------------------------------------------------------------*/
static int e820_compare(const void *a, const void *b)
{
   if (E820_BASE((const e820_range_t *)a) < E820_BASE((const e820_range_t *)b)) {
      return -1;
   }
   if (E820_BASE((const e820_range_t *)a) > E820_BASE((const e820_range_t *)b)) {
      return 1;
   }
   return 0;
}

/*-- e820_mmap_merge -----------------------------------------------------------
 *
 *      Merge memory map descriptors when they report contiguous memory of the
 *      same type.
 *
 * Parameters
 *      IN  mmap:  pointer to the memory map
 *      IN  count: original number of entries in the memory map
 *      OUT count: updated number of entries in the memory map
 *----------------------------------------------------------------------------*/
void e820_mmap_merge(e820_range_t *mmap, size_t *count)
{
   size_t idx, n;
   uint64_t len;

   if (*count > 1) {
      bubble_sort(mmap, *count, sizeof (e820_range_t), e820_compare);

      for (idx = 0; idx < *count - 1; idx++) {
         for (n = 0; n < *count - idx - 1; n++) {
            if (!IS_E820_MERGEABLE(&mmap[n], &mmap[n + 1])) {
               break;
            }
         }

         if (n > 0) {
            len = E820_BASE(&mmap[n]) + E820_LENGTH(&mmap[n]) - E820_BASE(mmap);
            e820_set_entry(mmap, E820_BASE(mmap), len, mmap->type,
                           mmap->attributes);

            if (idx + n + 1 < *count) {
               memmove(&mmap[1], &mmap[n + 1],
                       (*count - (idx + n + 1)) * sizeof (e820_range_t));
            }
            *count -= n;
         }

         mmap++;
      }
   }
}

/*-- e820_sanity_check ---------------------------------------------------------
 *
 *      Validate that the e820 is sorted and has no overlapping
 *      ranges.
 *
 * Parameters
 *      IN mmap:  pointer to e820 descriptors
 *      IN count: number of e820 descriptors
 *
 * Results
 *      ERR_SUCCESS or ERR_INVALID_PARAMETER.
 *----------------------------------------------------------------------------*/
static int e820_sanity_check(e820_range_t *mmap, size_t count)
{
   uint64_t max_base, max_limit, base, len, limit;
   bool error, overlap;
   const char *msg;
   size_t i;

   error = false;
   overlap = false;
   max_base = 0;
   max_limit = 0;

   for (i = 0; i < count; i++) {
      msg = NULL;
      base = E820_BASE(&mmap[i]);
      len = E820_LENGTH(&mmap[i]);
      limit = base + len - 1;

      if (!(i + 1 == count && base + len == 0) && (base + len < base)) {
         msg = "Memory map descriptor limit overflow\n";
      } else if (base < max_base) {
         msg = "Memory map is not sorted\n";
      }

      if (len > 0 && limit < max_limit) {
         overlap = true;
      }

      if (msg != NULL) {
         error = true;
         Log(LOG_ERR, "E820[%zu]: %"PRIx64" - %"PRIx64" type %u: %s\n",
             i, base, limit, mmap[i].type, msg);
      }

      max_base = base;
      max_limit = limit;
   }

   if (overlap || error) {
      for (i = 0; i < count; i++) {
         base = E820_BASE(&mmap[i]);
         limit = E820_BASE(&mmap[i]) + E820_LENGTH(&mmap[i]) - 1;
         Log(LOG_DEBUG, "E820[%zu]: %"PRIx64" - %"PRIx64" type %u\n",
             i, base, limit, mmap[i].type);
      }

      if (overlap) {
         Log(LOG_WARNING, "Memory map contains overlapping ranges\n");
      }

      if (error) {
         Log(LOG_ERR, "Memory map is corrupted.\n");
         return ERR_INVALID_PARAMETER;
      }
   }

   return ERR_SUCCESS;
}

/*-- e820_to_blacklist ---------------------------------------------------------
 *
 *      Blacklist memory that the memory map does not report as available:
 *        - memory above the last address reported in the memory map
 *        - undefined memory (holes in the memory map)
 *        - memory that is not reported as 'available' in the memory map
 *          (bootloader's memory is considered as 'available').
 *
 * Parameters
 *      IN mmap:  pointer to the system memory map
 *      IN count: number of entries in the memory map
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int e820_to_blacklist(e820_range_t *mmap, size_t count)
{
   uint64_t addr;
   int status;
   size_t i;

   Log(LOG_DEBUG, "Scanning system memory (%zu entries)...\n", count);

   status = e820_sanity_check(mmap, count);
   if (status != ERR_SUCCESS) {
      return status;
   }

   addr = E820_BASE(&mmap[count - 1]) + E820_LENGTH(&mmap[count - 1]) - 1;
   status = blacklist_runtime_mem(addr + 1, MAX_64_BIT_ADDR - addr);
   if (status != ERR_SUCCESS) {
      return status;
   }

   addr = 0;
   for (i = 0; i < count; i++) {
      if (E820_LENGTH(mmap) == 0) {
         continue;
      }

      if (E820_BASE(mmap) - addr > 0) {
         status = blacklist_runtime_mem(addr, E820_BASE(mmap) - addr);
         if (status != ERR_SUCCESS) {
            return status;
         }
      }

      if (mmap->type != E820_TYPE_AVAILABLE && mmap->type != E820_TYPE_BOOTLOADER) {
         status = blacklist_runtime_mem(E820_BASE(mmap), E820_LENGTH(mmap));
         if (status != ERR_SUCCESS) {
            return status;
         }
      }

      addr = E820_BASE(mmap) + E820_LENGTH(mmap);
      mmap++;
   }

   return ERR_SUCCESS;
}
