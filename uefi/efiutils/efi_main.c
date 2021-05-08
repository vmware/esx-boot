/*******************************************************************************
 * Copyright (c) 2008-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * efi_main.c -- Default app/driver entry point.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stack_chk.h>
#include "efi_private.h"

EXTERN int main(int argc, char **argv);

EFI_BOOT_SERVICES *bs = NULL;
EFI_RUNTIME_SERVICES *rs = NULL;
EFI_SYSTEM_TABLE *st = NULL;
EFI_HANDLE ImageHandle;


/*-- efi_main ------------------------------------------------------------------
 *
 *      This is the EFI-specific application entry point. This function
 *      initializes the firmware interface:
 *        1. Setup the ImageHandle, st and bs global variables so they point
 *           respectively to the current image handle, the UEFI system table,
 *           and the UEFI Boot Services dispatch structure.
 *        2. Reset the UEFI watchdog timer to 5 minutes.
 *        3. Initialize the memory allocator.
 *        4. Retrieve the command line arguments.
 *        5. Initialize ACPI support.
 *
 * Parameters
 *      IN Handle:      handle to the parent image
 *      IN SystemTable: pointer to the EFI system table
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS EFIAPI efi_main(EFI_HANDLE Handle, EFI_SYSTEM_TABLE *SystemTable)
{
   EFI_LOADED_IMAGE *Image;
   char **argv;
   int argc;
   int retval;
   EFI_STATUS Status;

   if (Handle == NULL || SystemTable == NULL ||
       SystemTable->BootServices == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   ImageHandle = Handle;
   st = SystemTable;
   bs = st->BootServices;
   rs = st->RuntimeServices;
   __stack_chk_init();

   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   Status = image_get_info(Handle, &Image);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   mem_init(Image->ImageDataType);

   Status = efi_create_argv(Handle, Image, &argc, &argv);
   if (Status != EFI_SUCCESS) {
      return Status;
   }

   acpi_init();
   tcg2_init();

   retval = main(argc, argv);
   Status = error_generic_to_efi(retval);

   efi_destroy_argv(argv);
   do_atexit();

   return Status;
}
