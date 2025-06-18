/*******************************************************************************
 * Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#ifndef GPXE_DOWNLOAD_H
#define GPXE_DOWNLOAD_H

/** @file
 *
 * gPXE Download Protocol
 *
 * EFI applications started by gPXE may use this interface to download files.
 */

typedef struct _GPXE_DOWNLOAD_PROTOCOL GPXE_DOWNLOAD_PROTOCOL;

/** Token to represent a currently downloading file */
typedef VOID *GPXE_DOWNLOAD_FILE;

/**
 * Callback function that is invoked when data arrives for a particular file.
 *
 * Not all protocols will deliver data in order. Clients should not rely on the
 * order of data delivery matching the order in the file.
 *
 * Some protocols are capable of determining the file size near the beginning
 * of data transfer. To allow the client to allocate memory more efficiently,
 * gPXE may give a hint about the file size by calling the Data callback with
 * a zero BufferLength and the file size in FileOffset. Clients should be
 * prepared to deal with more or less data than the hint actually arriving.
 *
 * @v Context		Context provided to the Start function
 * @v Buffer		New data
 * @v BufferLength	Length of new data in bytes
 * @v FileOffset	Offset of new data in the file
 * @ret Status		EFI_SUCCESS to continue the download,
 *			or any error code to abort.
 */
typedef
EFI_STATUS
(EFIAPI *GPXE_DOWNLOAD_DATA_CALLBACK)(
  IN VOID *Context,
  IN VOID *Buffer,
  IN UINTN BufferLength,
  IN UINTN FileOffset
  );

/**
 * Callback function that is invoked when the file is finished downloading, or
 * when a connection unexpectedly closes or times out.
 *
 * The finish callback is also called when a download is aborted by the Abort
 * function (below).
 *
 * @v Context		Context provided to the Start function
 * @v Status		Reason for termination: EFI_SUCCESS when the entire
 * 			file was transferred successfully, or an error
 * 			otherwise
 */
typedef
void
(EFIAPI *GPXE_DOWNLOAD_FINISH_CALLBACK)(
  IN VOID *Context,
  IN EFI_STATUS Status
  );

/**
 * Start downloading a file, and register callback functions to handle the
 * download.
 *
 * @v This		gPXE Download Protocol instance
 * @v Url		URL to download from
 * @v DataCallback	Callback that will be invoked when data arrives
 * @v FinishCallback	Callback that will be invoked when the download ends
 * @v Context		Context passed to the Data and Finish callbacks
 * @v File		Token that can be used to abort the download
 * @ret Status		EFI status code
 */
typedef
EFI_STATUS
(EFIAPI *GPXE_DOWNLOAD_START)(
  IN GPXE_DOWNLOAD_PROTOCOL *This,
  IN CHAR8 *Url,
  IN GPXE_DOWNLOAD_DATA_CALLBACK DataCallback,
  IN GPXE_DOWNLOAD_FINISH_CALLBACK FinishCallback,
  IN VOID *Context,
  OUT GPXE_DOWNLOAD_FILE *File
  );

/**
 * Forcibly abort downloading a file that is currently in progress.
 *
 * It is not safe to call this function after the Finish callback has executed.
 *
 * @v This		gPXE Download Protocol instance
 * @v File		Token obtained from Start
 * @v Status		Reason for aborting the download
 * @ret Status		EFI status code
 */
typedef
EFI_STATUS
(EFIAPI *GPXE_DOWNLOAD_ABORT)(
  IN GPXE_DOWNLOAD_PROTOCOL *This,
  IN GPXE_DOWNLOAD_FILE File,
  IN EFI_STATUS Status
  );

/**
 * Poll for more data from gPXE. This function will invoke the registered
 * callbacks if data is available or if downloads complete.
 *
 * @v This		gPXE Download Protocol instance
 * @ret Status		EFI status code
 */
typedef
EFI_STATUS
(EFIAPI *GPXE_DOWNLOAD_POLL)(
  IN GPXE_DOWNLOAD_PROTOCOL *This
  );

/**
 * The gPXE Download Protocol.
 *
 * gPXE will attach a gPXE Download Protocol to the DeviceHandle in the Loaded
 * Image Protocol of all child EFI applications.
 */
struct _GPXE_DOWNLOAD_PROTOCOL {
   GPXE_DOWNLOAD_START Start;
   GPXE_DOWNLOAD_ABORT Abort;
   GPXE_DOWNLOAD_POLL Poll;
};

#define GPXE_DOWNLOAD_PROTOCOL_GUID \
  { \
    0x3eaeaebd, 0xdecf, 0x493b, { 0x9b, 0xd1, 0xcd, 0xb2, 0xde, 0xca, 0xe7, 0x19 } \
  }

#endif
