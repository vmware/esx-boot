/*******************************************************************************
 * Copyright (c) 2020-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include <bootlib.h>
#include <boot_services.h>
#include <limits.h>
#include <stack_chk.h>

/*
 * stack_chk2.c -- Replaceable helper functions for gcc stack protection.
 */

/*-- __stack_chk_fail ----------------------------------------------------------
 *
 *      Compiler-generated stack smash checking code calls this function on
 *      failure.
 *----------------------------------------------------------------------------*/
void __attribute__((weak,noreturn)) __stack_chk_fail(void)
{
   int dummy;
   Log(LOG_EMERG, "Fatal error: Stack smash detected (sp=%p ra=%p)",
       &dummy, __builtin_return_address(0));
   while (1) {
      HLT();
   }
}

/*-- __stack_chk_fail_local ----------------------------------------------------
 *
 *      In some cases, compiler-generated stack smash checking code calls this
 *      function on failure instead of __stack_chk_fail.
 *----------------------------------------------------------------------------*/
void __attribute__((weak,noreturn,alias("__stack_chk_fail")))
   __stack_chk_fail_local(void);
