/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * atexit.c -- Simple implementation of POSIX atexit mechanism.
 */

#include <stdlib.h>

typedef struct exit_funcs_t {
   struct exit_funcs_t *next;
   void (*func)(void);
} exit_funcs_t;

static exit_funcs_t *exit_funcs;

/*-- atexit -------------------------------------------------------------------
 *
 *      Register a function to be called if this app or driver exits back to
 *      firmware.  Registered functions are not called if the app hands over
 *      the system to the OS via exit_boot_services.
 *
 * Parameters
 *      IN func: function to be called
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 * ---------------------------------------------------------------------------*/
int atexit(void (*func)(void))
{
   exit_funcs_t *ef = malloc(sizeof(exit_funcs_t));
   if (ef == NULL) {
      return -1;
   }
   ef->next = exit_funcs;
   ef->func = func;
   exit_funcs = ef;
   return 0;
}

/*-- do_atexit ----------------------------------------------------------------
 *
 *      Call the registered atexit functions, in reverse order of registration.
 * ---------------------------------------------------------------------------*/
void do_atexit(void)
{
   while (exit_funcs != NULL) {
      exit_funcs_t *ef;
      ef = exit_funcs;
      exit_funcs = ef->next;
      ef->func();
      free(ef);
   }
}
