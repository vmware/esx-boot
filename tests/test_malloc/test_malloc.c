/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_malloc.c -- test limits of malloc
 */

#include <stdlib.h>
#include <bootlib.h>
#include <compat.h>

#define MiB 0x100000

/*-- main ----------------------------------------------------------------------
 *
 *      test_malloc main function.
 *
 *      Allocates memory in 1 MiB chunks until malloc returns NULL.
 *      Records a failure if malloc never allocates memory above 4GB
 *      on a 64-bit system.  Cleans up by freeing the memory before
 *      exiting.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int main(UNUSED_PARAM(int argc), UNUSED_PARAM(char **argv))
{
   int status;
   void *last = NULL;
   int count = 0;
   int log_count_at = 1024;
   int res = ERR_SUCCESS;
   void *highest = NULL;

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   for (;;) {
      void *next = malloc(MiB);
      if (next == NULL) {
         break;
      }
      count++;
      *(void **)next = last;
      last = next;
      if (last > highest) {
         highest = last;
      }
      if (count == log_count_at) {
         Log(LOG_INFO, "Allocated %u MiB...", count);
         log_count_at = log_count_at << 1;
      }
   }

   Log(LOG_INFO, "Out of memory after allocating %u MiB", count);
   Log(LOG_INFO, "Highest allocation at address %p", highest);
   Log(LOG_INFO, "Last allocation at address    %p", last);

#if arch_is_64
   if ((uintptr_t)highest < 0x100000000) {
      Log(LOG_ERR, "FAILURE: 64-bit system, but could not allocate above 4GB");
      res = ERR_TEST_FAILURE;
   }
#endif

   Log(LOG_INFO, "Freeing memory...");
   while (last != NULL) {
      void *next = *(void **)last;
      free(last);
      last = next;
   }

   Log(LOG_INFO, "Done: %s", res == ERR_SUCCESS ? "Success" : "Failure");
   return res;
}
