/*******************************************************************************
 * Copyright (c) 2008-2023 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * sort.c -- Sorting routines
 */

#include <bootlib.h>

/*-- bubble_sort ---------------------------------------------------------------
 *
 *      Classic (non-optimized) bubble sort.
 *      This sorting algorithm is stable (it maintains the relative order of
 *      records with equal comparison keys).
 *
 * Parameters
 *      IN base:   pointer to the table to sort
 *      IN nmemb:  number of entries in the table
 *      IN size:   size of each entry
 *      IN compar: comparison function
 *----------------------------------------------------------------------------*/
void bubble_sort(void *base, size_t nmemb, size_t size,
                 int (*compar)(const void *, const void *))
{
   int swapped;
   char *e1, *e2;
   size_t i;

   if (nmemb > 1) {
      do {
         swapped = 0;
         for (i = 0, e1 = base; i < nmemb - 1; i++, e1 += size) {
            e2 = e1 + size;
            if (compar(e1, e2) > 0) {
               mem_swap(e1, e2, size);
               swapped = 1;
            }
         }
      } while (swapped);
   }
}
