/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
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
   log_init(false); // in case not initialized yet
   Log(LOG_EMERG, "Fatal error: Stack smash detected");
   while (1) {
      HLT();
   }
}

/*-- __stack_chk_fail_local ----------------------------------------------------
 *
 *      In some cases, compiler-generated stack smash checking code calls this
 *      function on failure instead of __stack_chk_fail.
 *----------------------------------------------------------------------------*/
void __attribute__((weak,noreturn)) __stack_chk_fail_local(void)
{
   __stack_chk_fail();
}
