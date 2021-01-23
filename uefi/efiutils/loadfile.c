/*******************************************************************************
 * Copyright (c) 2008-2011,2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * loadfile.c -- Accessing files using the Load File Protocol.
 */

#include "efi_private.h"

/*-- load_file_get_size --------------------------------------------------------
 *
 *      Get the size of a file using the Load File Protocol.
 *
 * Parameters
 *      IN  Volume:   handle to the volume on which the file is located.
 *      IN  filepath: absolute path to the file.
 *      OUT FileSize: the 64-bit file size, in bytes.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS load_file_get_size(EFI_HANDLE Volume, const char *filepath,
                              UINTN *FileSize)
{
   EFI_LOAD_FILE_INTERFACE *LoadFile;
   CHAR16 *FilePath;
   EFI_DEVICE_PATH *DevicePath;
   UINTN BufferSize;
   EFI_STATUS Status;

   Status = get_protocol_interface(Volume, &LoadFileProto, (void **)&LoadFile);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = filepath_unix_to_efi(filepath, &FilePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = file_devpath(Volume, FilePath, &DevicePath);
   sys_free(FilePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   BufferSize = 0;
   efi_set_watchdog_timer(WATCHDOG_DISABLE);
   Status = LoadFile->LoadFile(LoadFile, DevicePath, FALSE, &BufferSize, NULL);
   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);
   sys_free(DevicePath);
   if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
      return Status;
   }

   *FileSize = BufferSize;
   return EFI_SUCCESS;
}

/*-- load_file_load-------------------------------------------------------------
 *
 *      Load a file into memory using the Load File Protocol. UEFI watchdog
 *      timer is disabled during the LoadFile() operation, so it does not
 *      trigger and reboot the platform during large/slow file transfers.
 *
 * Parameters
 *      IN  Volume:   handle to the volume from which to load the file
 *      IN  filepath: absolute path to the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      IN  Buffer:   pointer to where to load the file
 *      IN  BufSize:  output buffer size in bytes
 *      OUT BufSize:  number of bytes that have been written into 'Buffer'
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS load_file_load(EFI_HANDLE Volume, const char *filepath,
                          int (*callback)(size_t), VOID **Buffer,
                          UINTN *BufSize)
{
   EFI_LOAD_FILE_INTERFACE *LoadFile;
   CHAR16 *FilePath;
   EFI_DEVICE_PATH *DevicePath;
   VOID *Data;
   UINTN Size;
   EFI_STATUS Status;
   int error;

   Status = get_protocol_interface(Volume, &LoadFileProto, (void **)&LoadFile);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = filepath_unix_to_efi(filepath, &FilePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = file_devpath(Volume, FilePath, &DevicePath);
   sys_free(FilePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   efi_set_watchdog_timer(WATCHDOG_DISABLE);
   Size = 0;
   Status = LoadFile->LoadFile(LoadFile, DevicePath, FALSE, &Size, NULL);
   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);
   if (EFI_ERROR(Status)) {
      sys_free(DevicePath);
      return Status;
   }

   Data = sys_malloc((size_t)Size);
   if (Data == NULL) {
      sys_free(DevicePath);
      return EFI_OUT_OF_RESOURCES;
   }

   efi_set_watchdog_timer(WATCHDOG_DISABLE);
   Status = LoadFile->LoadFile(LoadFile, DevicePath, FALSE, &Size, Data);
   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);
   sys_free(DevicePath);
   if (EFI_ERROR(Status)) {
      sys_free(Data);
      return Status;
   }

   /*
    * The progress callback should be called for every received packet, but the
    * Load File protocol does not support that, so just call once at the end.
    */
   if (callback != NULL) {
      error = callback(Size);
      if (error != 0) {
         sys_free(Data);
         return error_generic_to_efi(error);
      }
   }

   *Buffer = Data;
   *BufSize = Size;

   return EFI_SUCCESS;
}
