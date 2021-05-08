/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include <bootlib.h>
#include <boot_services.h>
#include <limits.h>
#include <stack_chk.h>
#include <cpu.h>

#if ULONG_MAX == UINT32_MAX
#define STACK_CHK_CANARY 0xde7ec7ed
#else
#define STACK_CHK_CANARY 0xde7ec7eddefea7ed
#endif

/*
 * stack_chk.c -- Helper functions for gcc stack protection.
 */

/*
 * Canary value that gcc will place on the stack at entry to functions and
 * check before return.  This variable must be kept in the bss section so that
 * modifying it early does not invalidate the crypto module's integrity hash.
 */
uintptr_t __stack_chk_guard;

/*-- __stack_chk_init ----------------------------------------------------------
 *
 *      One-time initialization for stack protection.
 *
 *      __attribute__((constructor)) works only in the COM32 build environment.
 *      In the UEFI environment, this function must be called explicitly from
 *      efi_main.
 *
 *      This function must not be compiled with stack protection, or its write
 *      to __stack_chk_guard would cause a spurious stack check failure upon
 *      return.  For compatibility with versions of gcc that do not yet
 *      implement __attribute__((no_stack_protector)), we compile this entire
 *      file with -fno-stack-protector; see the Makefile.
 *----------------------------------------------------------------------------*/
void
#if defined(__COM32__)
__attribute__((constructor))
#endif
__stack_chk_init(void)
{
   __stack_chk_guard = STACK_CHK_CANARY * (firmware_get_time_ms(false) | 1);
}
