/*******************************************************************************
 * Copyright (c) 2008-2011,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * gpxefile.c -- Provides access to files through the gPXE download protocol
 */

#include <string.h>
#include "efi_private.h"
#include "protocol/gpxe_download.h"

typedef struct {
   char *buffer;
   size_t size;
   BOOLEAN done;
   EFI_STATUS status;
   int (*callback)(size_t);
} GpxeCallbackContext;

/*-- has_gpxe_download_proto ---------------------------------------------------
 *
 *      Check whether the gPXE download protocol is available on a volume
 *
 * Parameters
 *      IN  Volume:   handle to the volume
 *
 * Results
 *      True if available, false if not
 *----------------------------------------------------------------------------*/
bool has_gpxe_download_proto(EFI_HANDLE Volume)
{
   EFI_GUID GpxeDownloadProto = GPXE_DOWNLOAD_PROTOCOL_GUID;
   GPXE_DOWNLOAD_PROTOCOL *gpxe;
   EFI_STATUS Status;

   Status = get_protocol_interface(Volume, &GpxeDownloadProto, (void **)&gpxe);
   return !EFI_ERROR(Status);
}

/*-- gpxe_download_data --------------------------------------------------------
 *
 *      Handle data arriving on a gPXE file.
 *
 * Parameters
 *      IN  Context:      callback context
 *      IN  Buffer:       the data received on the file
 *      IN  BufferLength: length of the data buffer
 *      IN  FileOffset:   offset in the file that the data is to be written at
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS EFIAPI gpxe_download_data(VOID *Context, VOID *Buffer,
                                            UINTN BufferLength,
                                            UINTN FileOffset)
{
   GpxeCallbackContext *context;
   int error;

   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   context = (GpxeCallbackContext *)Context;

   if (context->size < FileOffset + BufferLength) {
      char *tmp = sys_realloc(context->buffer, context->size,
                              FileOffset + BufferLength);
      if (tmp == NULL) {
         efi_set_watchdog_timer(WATCHDOG_DISABLE);
         return EFI_OUT_OF_RESOURCES;
      }

      context->buffer = tmp;
      context->size = FileOffset + BufferLength;
   }

   if (BufferLength > 0) {
      memcpy(context->buffer + FileOffset, Buffer, BufferLength);
   }

   if (context->callback != NULL) {
      error = context->callback(BufferLength);
      if (error != 0) {
         sys_free(context->buffer);
         efi_set_watchdog_timer(WATCHDOG_DISABLE);
         return error_generic_to_efi(error);
      }
   }

   efi_set_watchdog_timer(WATCHDOG_DISABLE);

   return EFI_SUCCESS;
}

/*-- gpxe_download_finished ----------------------------------------------------
 *
 *      Handle the end of data on a gPXE stream.
 *
 * Parameters
 *      IN  Context: callback context
 *      IN  Status:  EFI status code indicating success or failure
 *----------------------------------------------------------------------------*/
static void EFIAPI gpxe_download_finished(VOID *Context, EFI_STATUS Status)
{
   GpxeCallbackContext *context;

   context = (GpxeCallbackContext *)Context;
   context->done = TRUE;
   context->status = Status;
}

/*-- gpxe_file_load ------------------------------------------------------------
 *
 *      Load a file into memory using gPXE.
 *
 * Parameters
 *      IN  Volume:   handle to the volume from which to load the file
 *      IN  filepath: the ASCII absolute path of the file to retrieve
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      OUT Buffer:   pointer to where to load the file
 *      IN  BufSize:  output buffer size in bytes
 *      OUT BufSize:  number of bytes that have been written into Buffer
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS gpxe_file_load(EFI_HANDLE Volume, const char *filepath,
                          int (*callback)(size_t), VOID **Buffer,
                          UINTN *BufSize)
{
   EFI_GUID GpxeDownloadProto = GPXE_DOWNLOAD_PROTOCOL_GUID;
   GPXE_DOWNLOAD_PROTOCOL *gpxe;
   GPXE_DOWNLOAD_FILE file;
   GpxeCallbackContext context;
   EFI_STATUS Status;

   EFI_ASSERT_PARAM(filepath != NULL);
   EFI_ASSERT_PARAM(Buffer != NULL);
   EFI_ASSERT_PARAM(BufSize != NULL);

   Status = get_protocol_interface(Volume, &GpxeDownloadProto, (void **)&gpxe);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   context.buffer = NULL;
   context.size = 0;
   context.done = FALSE;
   context.callback = callback;

   Status = gpxe->Start(gpxe, (CHAR8*)filepath, gpxe_download_data,
                        gpxe_download_finished, &context, &file);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   while (!context.done) {
      efi_set_watchdog_timer(WATCHDOG_DISABLE);
      Status = gpxe->Poll(gpxe);
      efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

      if (EFI_ERROR(Status)) {
         sys_free(context.buffer);
         return Status;
      }
   }

   if (EFI_ERROR(context.status)) {
      sys_free(context.buffer);
      return context.status;
   }

   *Buffer = context.buffer;
   *BufSize = context.size;

   return EFI_SUCCESS;
}
