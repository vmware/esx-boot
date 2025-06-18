/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * simplefile.c -- Accessing files using the Simple File Protocol.
 */

#include "efi_private.h"

/* File loads and saves are in units of this much */
#define SIMPLEFILE_READ_BUFSIZE READ_CHUNK_SIZE
#define SIMPLEFILE_WRITE_BUFSIZE WRITE_CHUNK_SIZE

/*-- simple_file_volume_open ---------------------------------------------------
 *
 *      Open a volume using the Simple File Protocol.
 *
 * Parameters
 *      IN  Handle: handle to the volume to open
 *      OUT Volume: the volume interface
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS simple_file_volume_open(EFI_HANDLE Handle, EFI_FILE **Volume)
{
   EFI_FILE_IO_INTERFACE *fs;
   EFI_FILE *Vol;
   EFI_STATUS Status;

   Status = get_protocol_interface(Handle, &SimpleFileSystemProto,
                                   (void **)&fs);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = fs->OpenVolume(fs, &Vol);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   if (Vol == NULL) {
      return EFI_UNSUPPORTED;
   }

   *Volume = Vol;

   return Status;
}

/*-- simple_file_open ----------------------------------------------------------
 *
 *      Open a file using the Simple File Protocol.
 *
 *      NOTE: UEFI Specification v2.3 (12.5 "File Protocol") says:
 *            "The only valid combinations that the file may be opened with are:
 *             Read, Read/Write, or Create/Read/Write."
 *
 * Parameters
 *      IN  Volume:   handle to the volume on which the file is located
 *      IN  filepath: absolute path to the file
 *      IN  mode:     access mode
 *      OUT File:     handle to the file
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS simple_file_open(EFI_HANDLE Volume, const char *filepath,
                                   UINT64 mode, EFI_FILE_HANDLE *File)
{
   CHAR16 *FilePath;
   EFI_FILE *vol = NULL, *fd;
   EFI_STATUS Status;

   Status = simple_file_volume_open(Volume, &vol);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = filepath_unix_to_efi(filepath, &FilePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = vol->Open(vol, &fd, FilePath, mode, 0);
   vol->Close(vol);
   free(FilePath);

   if (EFI_ERROR(Status)) {
      return Status;
   } else if (fd == NULL) {
      return EFI_NOT_FOUND;
   }

   *File = fd;

   return EFI_SUCCESS;
}

/*-- simple_file_get_info ------------------------------------------------------
 *
 *      Wrapper for the GetInfo() method of the Simple File Protocol.
 *
 * Parameters
 *      IN  File:     handle to the file/volume to query
 *      IN  InfoType: pointer to the type GUID of the information to query
 *      OUT Info:     pointer to a the freshly allocated buffer containing the
 *                    info
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS simple_file_get_info(EFI_FILE_HANDLE File, EFI_GUID *InfoType,
                                       VOID **Info)
{
   EFI_FILE_INFO *buffer;
   UINTN buflen;
   EFI_STATUS Status;

   buffer = NULL;
   buflen = 0;

   do {
      if (buffer != NULL) {
         free(buffer);
      }

      if (buflen > 0) {
         buffer = malloc(buflen);
         if (buffer == NULL) {
            return EFI_OUT_OF_RESOURCES;
         }
      }

      Status = File->GetInfo(File, InfoType, &buflen, buffer);
      if (!EFI_ERROR(Status)) {
         if (buflen == 0 || buffer == NULL) {
            Status = EFI_UNSUPPORTED;
            break;
         }

         *Info = buffer;
         return EFI_SUCCESS;
      }
   } while (Status == EFI_BUFFER_TOO_SMALL);

   if (buffer != NULL) {
      free(buffer);
   }

   return Status;
}

/*-- simple_file_get_size ------------------------------------------------------
 *
 *      Get the size of a file using the Simple File Protocol.
 *
 * Parameters
 *      IN  Volume:   handle to the volume on which the file is located
 *      IN  filepath: absolute path to the file
 *      OUT FileSize: the 64-bit file size, in bytes
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS simple_file_get_size(EFI_HANDLE Volume, const char *filepath,
                                UINTN *FileSize)
{
   EFI_FILE_INFO *FileInfo;
   EFI_FILE *File;
   EFI_STATUS Status;

   Status = simple_file_open(Volume, filepath, EFI_FILE_MODE_READ, &File);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = simple_file_get_info(File, &GenericFileInfoId, (void **)&FileInfo);
   File->Close(File);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   *FileSize = (UINTN)FileInfo->FileSize;
   free(FileInfo);

   return EFI_SUCCESS;
}

/*-- simple_file_load ----------------------------------------------------------
 *
 *      Load a file into memory using the Simple File Protocol. UEFI watchdog
 *      timer is disabled during the file chunk transfers, so it does not
 *      trigger and reboot the platform during large/slow file transfers.
 *
 * Parameters
 *      IN  Volume:   handle to the volume on which the file is located
 *      IN  filepath: absolute path to the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      IN  Buffer:   pointer to where to load the file
 *      OUT BufSize:  number of bytes that have been written into Buffer
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS simple_file_load(EFI_HANDLE Volume, const char *filepath,
                            int (*callback)(size_t), VOID **Buffer,
                            UINTN *BufSize)
{
   EFI_FILE_INFO *FileInfo;
   EFI_FILE *File;
   EFI_STATUS Status;
   UINTN total_size, size, chunk_size;
   VOID *Data, *DataStart;
   int error;

   Status = simple_file_open(Volume, filepath, EFI_FILE_MODE_READ, &File);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = simple_file_get_info(File, &GenericFileInfoId, (void **)&FileInfo);
   if (EFI_ERROR(Status)) {
      File->Close(File);
      return Status;
   }

   size = (UINTN)FileInfo->FileSize;
   total_size = size;
   DataStart = malloc(size);
   if (DataStart == NULL) {
      File->Close(File);
      return EFI_OUT_OF_RESOURCES;
   }

   Status = EFI_SUCCESS;
   Data = DataStart;

   while (size > 0) {
      chunk_size = MIN(size, SIMPLEFILE_READ_BUFSIZE);

      efi_set_watchdog_timer(WATCHDOG_DISABLE);
      Status = File->Read(File, &chunk_size, Data);
      efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

      if (EFI_ERROR(Status)) {
         break;
      }

      Data = (char *)Data + chunk_size;
      size -= chunk_size;

      if (callback != NULL) {
         error = callback(chunk_size);
         if (error != 0) {
            Status = error_generic_to_efi(error);
            break;
         }
      }
   }

   File->Close(File);

   if (EFI_ERROR(Status)) {
      free(DataStart);
   } else {
      *Buffer = DataStart;
      *BufSize = total_size;
   }

   return Status;
}


/*-- simple_file_save ----------------------------------------------------------
 *
 *      Save a file from memory using the Simple File Protocol. UEFI watchdog
 *      timer is disabled during the file chunk transfers, so it does not
 *      trigger and reboot the platform during large/slow file transfers.
 *
 * Parameters
 *      IN  Volume:   handle to the volume on which the file is located
 *      IN  filepath: absolute path to the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    saved
 *      IN  Buffer:   pointer to buffer being saved
 *      IN  BufSize:  size of the buffer being saved
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS simple_file_save(EFI_HANDLE Volume, const char *filepath,
                            int (*callback)(size_t), VOID *Buffer,
                            UINTN BufSize)
{
   EFI_FILE *File;
   EFI_STATUS Status;
   UINTN size, chunk_size, written_size;
   VOID *Data;
   int error;

   Status = simple_file_open(Volume, filepath, EFI_FILE_MODE_READ |
                             EFI_FILE_MODE_WRITE, &File);
   if (!EFI_ERROR(Status)) {
      Log(LOG_WARNING, "%s: overwriting existing file", filepath);
      Status = File->Delete(File);
      if (EFI_ERROR(Status)) {
         return Status;
      }
      File = NULL;
   }

   Status = simple_file_open(Volume, filepath, EFI_FILE_MODE_READ |
                             EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, &File);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = EFI_SUCCESS;
   Data = Buffer;
   size = BufSize;

   while (size > 0) {
      chunk_size = MIN(size, SIMPLEFILE_WRITE_BUFSIZE);
      written_size = chunk_size;

      efi_set_watchdog_timer(WATCHDOG_DISABLE);
      Status = File->Write(File, &written_size, Data);
      efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

      if (EFI_ERROR(Status)) {
         break;
      }

      if (written_size != chunk_size) {
         /*
          * The spec is unclear if partial writes are "successful" or not.
          *
          * So let's be proactively safe here.
          */
         Log(LOG_WARNING, "%s: partial write", filepath);
         return EFI_DEVICE_ERROR;
      }

      Data = (char *)Data + chunk_size;
      size -= chunk_size;

      if (callback != NULL) {
         error = callback(chunk_size);
         if (error != 0) {
            Status = error_generic_to_efi(error);
            break;
         }
      }
   }

   File->Close(File);
   return Status;
}
