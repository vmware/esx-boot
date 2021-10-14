/*******************************************************************************
 * Copyright (c) 2020-2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * inboot.c -- EFI Firmware functions.
 * (Split from exitboot.c.)
 */

#include <bootlib.h>
#include "efi_private.h"

/*-- in_boot_services ----------------------------------------------------------
 *
 *      Return true if boot services are still available.
 *
 * Results
 *      true or false.
 *----------------------------------------------------------------------------*/
bool in_boot_services(void)
{
   return st->BootServices != NULL;
}
