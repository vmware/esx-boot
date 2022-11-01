/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * init_arch.c -- Architecture-specific EFI Firmware init/cleanup functions.
 */

#include <bootlib.h>
#include "efi_private.h"

/*-- sanitize_page_tables ------------------------------------------------------
 *
 *      Validate and transform MMU configuration to the state expected by
 *      allocate_page_tables and relocate_page_tables1/2.
 *
 *      Ensuring we have the full 4 levels of page tables present.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int sanitize_page_tables(void)
{
   return ERR_SUCCESS;
}
