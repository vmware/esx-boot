/*******************************************************************************
 * Copyright (c) 2008-2011,2013,2015,2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * isobounce.c --
 *
 *      This program is a simple chainloader for booting off of ISO9660
 *      formatted CDROM media.
 *
 *      EFI only provides a FAT filesystem driver natively. In order to boot
 *      from a CDROM, the EFI firmware looks at a special 'El Torito' boot
 *      entry, which tells it the starting LBA of a FAT filesystem image on the
 *      CDROM. The EFI shell and other EFI applications can access this FAT
 *      filesystem image, but they cannot access the rest of the contents of
 *      the CDROM.
 *
 *      This program gets around this limitation of the EFI firmware by
 *      installing an ISO9660 filesystem driver, then loading and transferring
 *      control to a second EFI application located in the ISO9660 filesystem.
 *      You put this program in a tiny FAT-formatted image that is pointed to
 *      by an El Torito boot entry, and you put your regular EFI bootloader,
 *      kernel images, initrds, kernel modules, and everything else in the
 *      ISO9660 filesystem. This is particularly useful if the kernel or
 *      modules can also be loaded by ISOLINUX (or another BIOS bootloader),
 *      because this way you do not end up with two copies of everything on the
 *      CD (in two different filesystems).
 */

#include <efiutils.h>
#include <bootlib.h>
#include <boot_services.h>

#ifndef ISO9660_DRIVER
   #if defined(only_arm64)
      #define ISO9660_DRIVER L"\\EFI\\DRIVERS\\ISO9660AA64.EFI"
   #elif defined(only_em64t)
      #define ISO9660_DRIVER L"\\EFI\\DRIVERS\\ISO9660x64.EFI"
   #else
      #define ISO9660_DRIVER L"\\EFI\\DRIVERS\\ISO9660IA32.EFI"
   #endif
#endif

#ifndef NEXT_LOADER
   #if defined(only_arm64)
      #define NEXT_LOADER L"EFI\\BOOT\\BOOTAA64.EFI"
   #elif defined(only_em64t)
      #define NEXT_LOADER L"EFI\\BOOT\\BOOTx64.EFI"
   #else
      #define NEXT_LOADER L"EFI\\BOOT\\BOOTIA32.EFI"
   #endif
#endif

int main(int argc, char **argv)
{
   EFI_HANDLE BootVolume, CdromDevice;
   EFI_HANDLE DriverImageHandle[2] = {NULL, NULL};
   EFI_STATUS Status, ChildStatus;
   int status;
   CHAR16 *LoadOptions;
   UINT32 LoadOptionsSize;

#if DEBUG
   log_init(true);
#else
   log_init(false);
#endif

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->ConnectController != NULL);
   EFI_ASSERT_FIRMWARE(bs->DisconnectController != NULL);

   LoadOptions = NULL;
   LoadOptionsSize = 0;

   /* Locate and load the ISO9660 driver */
   Status = get_boot_volume(&BootVolume);
   if (EFI_ERROR(Status)) {
      status = error_efi_to_generic(Status);
      Log(LOG_ERR, "get_boot_volume: %s", error_str[status]);
      return status;
   }
   log_handle_devpath(LOG_DEBUG, "BootVolume", BootVolume);

   Status = image_load(BootVolume, ISO9660_DRIVER, NULL, 0,
                       &DriverImageHandle[0], &ChildStatus);
   if (EFI_ERROR(Status)) {
      status = error_efi_to_generic(Status);
      Log(LOG_ERR, "image_load: %s", error_str[status]);
      return status;
   }
   if (EFI_ERROR(ChildStatus)) {
      status = error_efi_to_generic(ChildStatus);
      Log(LOG_ERR, "image_load (child status): %s", error_str[status]);
      return status;
   }

   /* Disconnect all drivers from the CDROM, then connect the ISO9660 driver. */
   Status = get_boot_device(&CdromDevice);
   if (EFI_ERROR(Status)) {
      status = error_efi_to_generic(Status);
      Log(LOG_ERR, "get_boot_device: %s", error_str[status]);
      return status;
   }
   log_handle_devpath(LOG_DEBUG, "CdromDevice", CdromDevice);

   Status = bs->DisconnectController(CdromDevice, NULL, NULL);
   if (EFI_ERROR(Status)) {
      status = error_efi_to_generic(Status);
      Log(LOG_ERR, "DisconnectController: %s", error_str[status]);
      return status;
   }

   Status = bs->ConnectController(CdromDevice, DriverImageHandle, NULL, FALSE);
   if (EFI_ERROR(Status)) {
      status = error_efi_to_generic(Status);
      Log(LOG_ERR, "ConnectController: %s", error_str[status]);
      return status;
   }

   /* Get the Load Options, if any, to be passed to the next boot loader */
   if (argc > 1 && argv != NULL) {
      /* The first argument i.e. argv[0] has the executable name */
      Status = argv_to_ucs2(argc - 1, argv + 1, &LoadOptions);
      if (EFI_ERROR(Status)) {
         status = error_efi_to_generic(Status);
         Log(LOG_ERR, "argv_to_ucs2: %s", error_str[status]);
         return status;
      }
      LoadOptionsSize = (UINT32)UCS2SIZE(LoadOptions);
   }

   /* Chainload the bootloader */
   Status = image_load(CdromDevice, NEXT_LOADER, LoadOptions, LoadOptionsSize,
                       NULL, &ChildStatus);
   if (EFI_ERROR(Status)) {
      status = error_efi_to_generic(Status);
      Log(LOG_ERR, "image_load: %s", error_str[status]);
      return status;
   }
   if (EFI_ERROR(ChildStatus)) {
      status = error_efi_to_generic(ChildStatus);
      Log(LOG_ERR, "image_load (child status): %s", error_str[status]);
      return status;
   }

   efi_free(LoadOptions);
   return error_efi_to_generic(EFI_SUCCESS);
}
