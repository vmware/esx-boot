/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * image.c -- Loaded Image management
 */

#include <string.h>
#include "efi_private.h"

#define EFI_BOOT_LOADED    TRUE
#define EFI_CHAIN_LOADED   FALSE

static EFI_GUID LoadedImageProto = LOADED_IMAGE_PROTOCOL;

/*-- image_get_info ------------------------------------------------------------
 *
 *      Get the protocol interface of a loaded image.
 *
 * Parameters
 *      IN  Handle: handle to the image to query
 *      OUT Image:  pointer to the Image protocol interface
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS image_get_info(EFI_HANDLE Handle, EFI_LOADED_IMAGE **Image)
{
   EFI_LOADED_IMAGE *LoadedImage;
   EFI_STATUS Status;

   Status = get_protocol_interface(Handle, &LoadedImageProto,
                                   (void **)&LoadedImage);

   if (!EFI_ERROR(Status)) {
      *Image = LoadedImage;
   }

   return Status;
}

/*-- image_load ----------------------------------------------------------------
 *
 *      Load and execute an EFI image from the specified volume and path.
 *
 *      UEFI Specification v2.3 (4.1 "UEFI Image Entry Point") says:
 *
 *      "An applications written to this specification is always unloaded from
 *       memory when it exits, and its return status is returned to the
 *       component that started the application."
 *
 *      "If a driver returns an error, then the driver is unloaded from memory.
 *       If the driver returns EFI_SUCCESS, then it stays resident in memory."
 *
 *      Therefore, there is no need to unload the image manually if it has been
 *      successfully started.
 *
 * Parameters
 *      IN  volume:    the handle of the volume to load the image from
 *      IN  FilePath:  the image path
 *      IN  optbuf:    pointer to the option buffer
 *      IN  optsize:   option buffer size in bytes
 *      OUT DrvHandle: Driver image handle (does not apply to applications)
 *      OUT Retval:    the application return value
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *
 * Side Effects
 *     Specific to the command line.
 *----------------------------------------------------------------------------*/
EFI_STATUS image_load(EFI_HANDLE Volume, const CHAR16 *FilePath, VOID *OptBuf,
                      UINT32 OptSize, EFI_HANDLE *DrvHandle, EFI_STATUS *RetVal)
{
   EFI_DEVICE_PATH *DevPath;
   EFI_LOADED_IMAGE *Child;
   EFI_HANDLE ChildHandle;
   EFI_STATUS Status;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->LoadImage != NULL);
   EFI_ASSERT_FIRMWARE(bs->StartImage != NULL);
   EFI_ASSERT_FIRMWARE(bs->UnloadImage != NULL);

   /* Load the EFI image into memory */
   Status = file_devpath(Volume, FilePath, &DevPath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = bs->LoadImage(EFI_CHAIN_LOADED, ImageHandle, DevPath, NULL, 0,
                          &ChildHandle);
   sys_free(DevPath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   /* Pass the command line and system table to the child */
   Status = image_get_info(ChildHandle, &Child);
   if (EFI_ERROR(Status)) {
      bs->UnloadImage(ChildHandle);
      return Status;
   }

   Child->LoadOptions = OptBuf;
   Child->LoadOptionsSize = OptSize;
   Child->SystemTable = st;

   /* Transfer control to the Child */
   Status = bs->StartImage(ChildHandle, NULL, NULL);
   if (Status == EFI_INVALID_PARAMETER) {
      bs->UnloadImage(ChildHandle);
      return Status;
   }

   if (DrvHandle != NULL) {
      EFI_ASSERT(Child->ImageCodeType == EfiBootServicesCode ||
                 Child->ImageCodeType == EfiRuntimeServicesCode);
      *DrvHandle = ChildHandle;
   }

   if (RetVal != NULL) {
      *RetVal = Status;
   }

   return EFI_SUCCESS;
}
