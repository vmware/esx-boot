/*******************************************************************************
 * Copyright (c) 2008-2011,2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * intcall.c -- Interrupt call wrappers
 */

#include <string.h>
#include <cpu.h>
#include "com32_private.h"

/*-- intcall -------------------------------------------------------------------
 *
 *      Generic BIOS call wrapper.
 *
 * Parameters
 *      IN vector: interrupt #
 *      IN iregs:  pointer to the input registers structure
 *      IN oregs:  pointer to the output registers structure
 *----------------------------------------------------------------------------*/
void intcall(uint8_t vector, const com32sys_t *iregs, com32sys_t *oregs)
{
   com32sys_t tmpiregs, tmporegs;

   if (!com32.in_boot_services) {
      return;
   }

   if (iregs == NULL) {
      memset(&tmpiregs, 0, sizeof (com32sys_t));
      iregs = &tmpiregs;
   }

   if (oregs == NULL) {
      oregs = &tmporegs;
   }

   memset(oregs, 0, sizeof (com32sys_t));
   __com32.cs_intcall(vector, iregs, oregs);
}

/*-- intcall_check_CF ----------------------------------------------------------
 *
 *      BIOS call wrapper which returns an error if CF=1 on exit.
 *
 * Parameters
 *      IN vector: interrupt #
 *      IN iregs:  pointer to the input registers structure
 *      IN oregs:  pointer to the output registers structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int intcall_check_CF(uint8_t vector, const com32sys_t *iregs, com32sys_t *oregs)
{
   com32sys_t tmpregs;

   if (!com32.in_boot_services) {
      return ERR_NOT_READY;
   }

   if (oregs == NULL) {
      oregs = &tmpregs;
   }

   intcall(vector, iregs, oregs);

   if (oregs->eflags.l & EFLAGS_CF) {
      return ERR_UNSUPPORTED;
   }

   return ERR_SUCCESS;
}
