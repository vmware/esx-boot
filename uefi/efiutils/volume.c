/*******************************************************************************
 * Copyright (c) 2008-2011,2013-2014,2016,2019-2020 VMware, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/* volume.c -- Volumes, partitions and boot device management
 *
 *    "volume"
 *       A volume abstracts any source from which we can load a file. This is
 *       typically a partition on a hard drive, on a USB stick or on a CDROM,
 *       but it can also be a raw disk, or a socket to a network device.
 *
 *    "boot volume"
 *       This is the volume from which the bootloader was loaded.
 *
 *    "boot directory"
 *       This is the directory, on the boot volume, from which the bootloader
 *       was loaded.
 *
 *    "boot device"
 *       This is the hardware device that is containing the boot volume.
 *       Example: considering that /dev/hda1 is the boot volume, then /dev/hda
 *       is the boot device.
 */

#include <string.h>
#include <libgen.h>
#include "efi_private.h"

/*-- get_boot_file ------------------------------------------------------------
 *
 *      Get the pathname of the boot file.
 *
 *      NOTE: If the Image FilePath is NULL then the boot filename is an empty
 *      string.
 *
 * Parameter
 *      OUT buffer: pointer to the freshly allocated file path
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_boot_file(char **buffer)
{
   EFI_LOADED_IMAGE *Image;
   EFI_PXE_BASE_CODE *Pxe;
   EFI_STATUS Status;
   CHAR16 *Path;
   char *path, *p;
   int i;

   if (is_http_boot()) {
      return get_http_boot_url(buffer);
   }

   Pxe = NULL;

   Status = image_get_info(ImageHandle, &Image);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (Image->FilePath != NULL) {
      Status = devpath_get_filepath(Image->FilePath, &Path);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }

      if (Path[0] == L'\0' && is_pxe_boot(&Pxe)) {
         /* during PXE Boot, MEDIA FilePath is empty, and in order to get the
          * boot file, we need to query the PXE BASE CODE PROTOCOL
          */
         sys_free(Path);
         Path = NULL;
         Status = get_pxe_boot_file(Pxe, &Path);
         if (EFI_ERROR(Status)) {
            return error_efi_to_generic(Status);
         }
      }

      path = (char *)Path;
      Status = ucs2_to_ascii(Path, &path, true);
      if (EFI_ERROR(Status)) {
         /*
          * Ignore path if it looks like garbage, instead of failing
          * the whole boot.
          */
         Log(LOG_WARNING, "Bootfile pathname appears invalid; ignoring");
         path = strdup("");
      }

      /*
       * If the path was a URL, it may have been damaged by the round-trip
       * conversion from ASCII to a devpath and back.  For example, a leading
       * http:// may have become \http:\.  Repair it as best we can.
       */
      if (path[0] == '\\' && (p = strstr(path, ":\\")) != NULL) {
         /*
          * Copy the URL scheme, colon, and single trailing backslash one byte
          * backward, thus overwriting the unwanted leading backslash and
          * leaving two trailing backslashes.
          */
         memmove(path, path + 1, p - path + 1);
      }
      for (i = 0; path[i] != '\0'; i++) {
         if (path[i] == '\\') {
            path[i] = '/';
         }
      }
   } else {
      path = strdup("");
      if (path == NULL) {
         return error_efi_to_generic(EFI_OUT_OF_RESOURCES);
      }
   }

   *buffer = path;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- get_boot_dir --------------------------------------------------------------
 *
 *      Get the pathname of the boot directory.
 *
 * Parameter
 *      OUT buffer: a pointer to the freshly allocated path.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_boot_dir(char **buffer)
{
   char *dirpath, *path;
   int status;

   if (buffer == NULL) {
      return error_efi_to_generic(EFI_INVALID_PARAMETER);
   }

   status = get_boot_file(&path);
   if (status != error_efi_to_generic(EFI_SUCCESS)) {
      return status;
   }

   dirpath = strdup(dirname(path));
   efi_free(path);
   if (dirpath == NULL) {
      return error_efi_to_generic(EFI_OUT_OF_RESOURCES);
   }

   if (strcmp(dirpath, ".") == 0) {
      /* If the boot file name is empty, NULL or is a plain file name */
      *dirpath = '/';
   }

   *buffer = dirpath;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- get_boot_volume -----------------------------------------------------------
*
 *      Get the boot volume handle.
 *
 * Parameters
 *      OUT Volume: the boot volume handle
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_boot_volume(EFI_HANDLE *Volume)
{
   EFI_LOADED_IMAGE *Image;
   EFI_STATUS Status;

   Status = image_get_info(ImageHandle, &Image);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   *Volume = Image->DeviceHandle;

   return EFI_SUCCESS;
}

/*-- get_boot_device -----------------------------------------------------------
 *
 *      Get a handle for the hardware device or virtual hardware device (hard
 *      drive, USB stick, CDROM drive, network device, ramdisk...) we were
 *      booted from.
 *
 *      It is found with the remaining part of the boot volume device path,
 *      once the trailing MEDIA_DEVICE_PATH nodes (other than ramdisks!)
 *      have been stripped off.
 *
 *      Ramdisks are special because even though they have type
 *      MEDIA_DEVICE_PATH, they act as virtual hardware, not media.  The device
 *      path for an ISO image ramdisk models the ramdisk as a VirtualCD drive
 *      with a CDROM mounted in it.  Example:
 *
 *      PciRoot(0x0)/Pci(0x1C,0x0)/Pci(0x0,0x1)/MAC(D06726D151E9,0x1)/
 *       IPv4(0.0.0.0,TCP,DHCP,192.168.53.128,192.168.53.1,255.255.255.0)/
 *       Uri(http://...)/VirtualCD(0x7A8BD000,0x86411FFF,0)/
 *       CDROM(0x1,0x106C,0x5CA3C)
 *
 *      For isobounce to work with a path like the above, get_boot_device must
 *      strip the CDROM node from the path but leave the VirtualCD node in
 *      place.  The ISO9660 driver can be connected to the VirtualCD node, but
 *      not to the Uri node above it.  See PR 2173724.
 *
 * Parameters
 *      OUT Device: the boot device handle
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_boot_device(EFI_HANDLE *Device)
{
   EFI_DEVICE_PATH *node, *DevPath;
   EFI_HANDLE BootVolume;
   EFI_STATUS Status;

   Status = get_boot_volume(&BootVolume);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = devpath_get(BootVolume, &DevPath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = devpath_duplicate(DevPath, &DevPath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   FOREACH_DEVPATH_NODE(DevPath, node) {
      if (node->Type == MEDIA_DEVICE_PATH &&
          node->SubType != MEDIA_RAM_DISK_DP) {
         SetDevPathEndNode(node);
         break;
      }
   }

   Status = devpath_handle(DevPath, Device);

   sys_free(DevPath);
   return Status;
}
