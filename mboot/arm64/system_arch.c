/*******************************************************************************
 * Copyright (c) 2017-2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * system_arch.c -- Various architecture-specific system routines.
 */

#include <cpu.h>
#include <boot_services.h>
#include "mboot.h"

/*-- system_arch_blacklist_memory ----------------------------------------------
 *
 *      Blacklist architecture-specific memory ranges.
 *
 * Parameters
 *      None.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int system_arch_blacklist_memory(void)
{
   return ERR_SUCCESS;
}

void check_cpu_quirks(void)
{
   return;
}
