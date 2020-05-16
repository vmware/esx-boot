/*******************************************************************************
 * Copyright (c) 2018,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * esxbootinfo_arch.c -- Arch-specific portions of esxbootinfo.
 */

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <esxbootinfo.h>
#include <stdbool.h>
#include <cpu.h>


/*-- esxbootinfo_arch_supported_req_flags --------------------------------------
 *
 *      Extra arch-specific supported required flags.
 *
 * Parameters
 *      None.
 *
 * Results
 *      0.
 *----------------------------------------------------------------------------*/
int esxbootinfo_arch_supported_req_flags(void)
{
   return 0;
}

/*-- esxbootinfo_arch_check_kernel----------------------------------------------
 *
 *      Extra arch-specific kernel checks.
 *
 * Parameters
 *      IN mbh: ESXBootInfo header.
 *
 * Results
 *      False if kernel is not supported (with error logged).
 *----------------------------------------------------------------------------*/
bool esxbootinfo_arch_check_kernel(UNUSED_PARAM(ESXBootInfo_Header *mbh))
{
   return true;
}
