/*******************************************************************************
 * Copyright (c) 2018,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * esxbootinfo_arch.c -- Arch-specific portions of ESXBootInfo.
 */

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <esxbootinfo.h>
#include <stdbool.h>
#include <cpu.h>
#include <bootlib.h>

/*-- esxbootinfo_arch_supported_req_flags --------------------------------------
 *
 *      Extra arch-specific supported required flags.
 *
 * Parameters
 *      None.
 *
 * Results
 *      ESXBOOTINFO_ARCH_FLAG_ARM64_EL1.
 *----------------------------------------------------------------------------*/
int esxbootinfo_arch_supported_req_flags(void)
{
   return ESXBOOTINFO_ARCH_FLAG_ARM64_EL1;
}

/*-- esxbootinfo_arch_check_kernel----------------------------------------------
 *
 *      Extra arch-specific kernel checks.
 *
 * Parameters
 *      IN mbh: ESXBootIinfo header.
 *
 * Results
 *      False if kernel is not supported (with error logged).
 *----------------------------------------------------------------------------*/
bool esxbootinfo_arch_check_kernel(ESXBootInfo_Header *mbh)
{
   unsigned system_el = el_is_hyp() ? 2 : 1;
   unsigned kernel_el =
      (mbh->flags & ESXBOOTINFO_ARCH_FLAG_ARM64_EL1) == 0 ? 2 : 1;

   if (system_el != kernel_el) {
      Log(LOG_ERR, "System EL(%u) != kernel EL(%u) (ESXBootInfo flags 0x%x)\n",
          system_el, kernel_el, mbh->flags);
      return false;
   }

   return true;
}
