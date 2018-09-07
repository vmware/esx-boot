/*******************************************************************************
 * Copyright (c) 2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mutiboot_arch.c -- Arch-specific portions of mutiboot.
 */

#include <string.h>
#include <stdio.h>
#include <mutiboot.h>
#include <stdbool.h>
#include <cpu.h>


/*-- mutiboot_arch_supported_req_flags -----------------------------------------
 *
 *      Extra arch-specific supported required flags.
 *
 * Parameters
 *      None.
 *
 * Results
 *      0.
 *----------------------------------------------------------------------------*/
int mutiboot_arch_supported_req_flags(void)
{
   return 0;
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
bool mutiboot_arch_check_kernel(UNUSED_PARAM(Mutiboot_Header *mbh))
{
   return true;
}
