/*******************************************************************************
 * Copyright (c) 2018,2020,2021 VMware, Inc.  All rights reserved.
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
   bool kernel_vhe = (mbh->flags & ESXBOOTINFO_ARCH_FLAG_ARM64_VHE) != 0;

   if (el_is_hyp()) {
      if (vhe_enabled()) {
         /*
          * Some CPUs can only run in VHE mode, and thus we'll enter esxboot
          * with the E2H bit set in HCR_EL2. These CPUs can only run OSes
          * that are meant to run in VHE mode.
          */
         if (!kernel_vhe) {
            Log(LOG_ERR, "This system only supports VHE-enabled kernels\n");
            return false;
         }

         return true;
      } else if (vhe_supported() && kernel_vhe) {
         /*
          * VHE is not enabled, but VHE is supported by the CPU.
          *
          * Allow any kernel with VHE support, regardless of the
          * ESXBOOTINFO_ARCH_FLAG_ARM64_EL1 flag.
          */
         return true;
      }
   }

   /*
    * No VHE support in CPU or kernel. Fall back to checking the architecture
    * EL1/EL2 flag.
    */

   if (system_el != kernel_el) {
      Log(LOG_ERR, "System EL(%u) != kernel EL(%u) (ESXBootInfo flags 0x%x)\n",
          system_el, kernel_el, mbh->flags);
      return false;
   }

   return true;
}
