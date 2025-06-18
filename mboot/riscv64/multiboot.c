/*******************************************************************************
 * Copyright (c) 2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * multiboot.c -- There is no legacy Multiboot support on RISCV64 platforms.
 */

#include <string.h>
#include <stdio.h>
#include <multiboot.h>
#include <stdbool.h>
#include <e820.h>
#include <boot_services.h>

#include "mboot.h"

/*-- check_multiboot_kernel ----------------------------------------------------
 *
 *      Check whether the given buffer contains a valid Multiboot kernel.
 *
 * Parameters
 *      IN kbuf:  pointer to the kernel buffer
 *      IN ksize: kernel buffer size, in bytes
 *
 * Results
 *      ERR_UNSUPPORTED.
 *----------------------------------------------------------------------------*/
int check_multiboot_kernel(UNUSED_PARAM(void *kbuf), UNUSED_PARAM(size_t ksize))
{
   return ERR_UNSUPPORTED;
}

/*-- mb_mmap_desc_size ---------------------------------------------------------
 *
 *      Return the size of a Multiboot memory map entry. The returned size
 *      includes the 4 bytes of the 'size' descriptor field.
 *
 *      This is a dead code path. Never called.
 *
 * Results
 *      0.
 *----------------------------------------------------------------------------*/
size_t mb_mmap_desc_size(void)
{
   return 0;
}

/*-- multiboot_set_runtime_pointers --------------------------------------------
 *
 *      1) Destructively convert boot.mmap from e820 format to multiboot format.
 *
 *      2) Setup the Multiboot Info structure internal pointers to their
 *      run-time (relocated) values.
 *
 *      This is a dead code path. Never called.
 *
 * Parameters
 *      OUT run_mbi: address of the relocated Multiboot Info structure.
 *
 * Results
 *      ERR_UNSUPPORTED.
 *----------------------------------------------------------------------------*/
int multiboot_set_runtime_pointers(UNUSED_PARAM(run_addr_t *run_mbi))
{
   return ERR_UNSUPPORTED;
}

/*-- multiboot_init ------------------------------------------------------------
 *
 *      Allocate the Multiboot Info structure.
 *
 *      This is a dead code path. Never called.
 *
 * Results
 *      ERR_UNSUPPORTED.
 *----------------------------------------------------------------------------*/
int multiboot_init(void)
{
   return ERR_UNSUPPORTED;
}

/*-- multiboot_register -------------------------------------------------------
 *
 *      Register the objects that will need to be relocated.
 *
 *      This is a dead code path. Never called.
 *
 * Results
 *      ERR_UNSUPPORTED.
 *----------------------------------------------------------------------------*/
int multiboot_register(void)
{
   return ERR_UNSUPPORTED;
}
