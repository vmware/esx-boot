/*******************************************************************************
 * Copyright (c) 2018,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mutiboot_arch.c -- Arch-specific portions of mutiboot.
 */

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <mutiboot.h>
#include <stdbool.h>
#include <cpu.h>
#include <bootlib.h>

/*-- mutiboot_arch_supported_req_flags -----------------------------------------
 *
 *      Extra arch-specific supported required flags.
 *
 * Parameters
 *      None.
 *
 * Results
 *      MUTIBOOT_ARCH_FLAG_ARM64_EL1.
 *----------------------------------------------------------------------------*/
int mutiboot_arch_supported_req_flags(void)
{
   return MUTIBOOT_ARCH_FLAG_ARM64_EL1;
}

/*-- mutiboot_arch_check_kernel-------------------------------------------------
 *
 *      Extra arch-specific kernel checks.
 *
 * Parameters
 *      IN mbh: Mutiboot header.
 *
 * Results
 *      False if kernel is not supported (with error logged).
 *----------------------------------------------------------------------------*/
bool mutiboot_arch_check_kernel(Mutiboot_Header *mbh)
{
   unsigned system_el = el_is_hyp() ? 2 : 1;
   unsigned kernel_el = (mbh->flags & MUTIBOOT_ARCH_FLAG_ARM64_EL1) == 0 ? 2 : 1;

   if (system_el != kernel_el) {
      Log(LOG_ERR, "System EL(%u) != kernel EL(%u) (Mutiboot flags 0x%x)\n",
          system_el, kernel_el, mbh->flags);
      return false;
   }

   return true;
}
