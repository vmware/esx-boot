/*******************************************************************************
 * Copyright (c) 2008-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tftpfile.c --
 *
 *      Provides access to files through the (M)TFTP and PXE Base Code (PXE BC)
 *      Protocols.
 *
 *      Ideally, all files could be loaded using the Load File Protocol,
 *      regardless of the underlying media/transport type. However, the EFI
 *      spec explicitly states that the PXE BC's implementation of LoadFile()
 *      may only be used to discover and retrieve the bootstrap program
 *      (i.e. this bootloader). Other files (e.g. the kernel) must be retrieved
 *      using explicit TFTP calls. That's the purpose of this file.
 *
 *      The following code does not perform a complete PXE exchange. Instead,
 *      the assumption is made that if this is a PXE boot, the PXE BC and MTFTP
 *      modules must have been initialized and the PXE protocol carried out in
 *      order to discover and download this program. Therefore, after proper
 *      sanity checks, the TFTP functionality is accessed directly.
 */

#include "efi_private.h"
/*
 * TFTP block size to request (RFC 2348).  The server can always choose to use
 * a smaller size.  Using a large block size makes transfers faster by
 * increasing the amount of data that is transferred per ack.  It also allows
 * larger files to be transferred without sequence number wrapping, which not
 * all servers support.  On the other hand, if the block size is larger than
 * the path MTU, the blocks will be fragmented at the IP level, so if the
 * network is lossy, that increases the probability of the block needing to be
 * retransmitted because a fragment was lost.
 *
 * The default value set here can be overridden by calling tftp_set_block_size.
 *
 * Contrary to the EFI spec, the EDK implementation of Mtftp() will not
 * negotiate the largest block size with the server if the BlockSize argument
 * is NULL.  Further, the elilo sources mention that some real firmware
 * implementations timeout when given a NULL BlockSize.  Therefore, we always
 * pass in an explicit BlockSize request, never NULL.
 */
#define TFTP_BLKSIZE_MIN 512   // defined by UEFI standard
#define TFTP_BLKSIZE_MAX 65464 // defined by RFC 2348
static UINTN tftp_block_size = 1468; // default; fits in 1500 byte MTU

/*-- tftp_set_block_size  ------------------------------------------------------
 *
 *      Set the blksize option value to be used in TFTP requests.
 *
 * Parameters
 *      IN size:  block size
 *----------------------------------------------------------------------------*/
void tftp_set_block_size(size_t blksize)
{
   if (blksize < TFTP_BLKSIZE_MIN || blksize > TFTP_BLKSIZE_MAX) {
      Log(LOG_WARNING,
          "Requested TFTP blksize %zu not in range %u-%u; using %zu",
          blksize, TFTP_BLKSIZE_MIN, TFTP_BLKSIZE_MAX, tftp_block_size);
      return;
   }
   Log(LOG_DEBUG, "Switching TFTP blksize from %zu to %zu",
       tftp_block_size, blksize);
   tftp_block_size = blksize;
}

/*-- tftp_file_get_size --------------------------------------------------------
 *
 *      Get the size of a file using TFTP.
 *
 *      If downloading from a TFTP server without the 'tsize' option, it is
 *      possible that this could end up downloading the whole file and throwing
 *      its contents away. Get yourself a better TFTP server!
 *
 * Parameters
 *      IN  Volume:   handle to the volume from which to load the file
 *      IN  filepath: the ASCII absolute path of the file to retrieve
 *      OUT FileSize: file size in bytes
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS tftp_file_get_size(UNUSED_PARAM(EFI_HANDLE Volume),
                              const char *filepath, UINTN *FileSize)
{
   EFI_PXE_BASE_CODE *Pxe;
   EFI_IP_ADDRESS ServerIp;
   /* Some firmware doesn't like a NULL BufferPtr in the call to Mtftp(). */
   UINT8 DummyBuf;
   UINT64 Size = 0;
   EFI_STATUS Status;

   EFI_ASSERT_PARAM(filepath != NULL);
   EFI_ASSERT_PARAM(FileSize != NULL);

   Status = get_pxe_info(&Pxe, &ServerIp);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   efi_set_watchdog_timer(WATCHDOG_DISABLE);

   Status = Pxe->Mtftp(Pxe, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
                       &DummyBuf, FALSE, &Size, &tftp_block_size,
                       &ServerIp, (UINT8 *)filepath, NULL, TRUE);

   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   /*
    * Some firmware returns EFI_BUFFER_TOO_SMALL even though it successfully
    * managed to get the size.
    */
   if (Status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(Status)) {
      return Status;
   }

   *FileSize = (UINTN)Size;

   return EFI_SUCCESS;
}

/*-- tftp_file_load ------------------------------------------------------------
 *
 *      Load a file into memory using TFTP. UEFI watchdog timer is disabled
 *      during the Mtftp() operation, so it does not trigger and reboot the
 *      platform during large/slow file transfers.
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
EFI_STATUS tftp_file_load(EFI_HANDLE Volume, const char *filepath,
                          int (*callback)(size_t), VOID **Buffer,
                          UINTN *BufSize)
{
   EFI_PXE_BASE_CODE *Pxe;
   EFI_IP_ADDRESS ServerIp;
   VOID *Data;
   UINTN Size;
   UINT64 Size64;
   EFI_STATUS Status;
   int error;

   EFI_ASSERT_PARAM(filepath != NULL);
   EFI_ASSERT_PARAM(Buffer != NULL);
   EFI_ASSERT_PARAM(BufSize != NULL);

   Status = tftp_file_get_size(Volume, filepath, &Size);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = get_pxe_info(&Pxe, &ServerIp);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Data = malloc(Size);
   if (Data == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   Size64 = Size;
   efi_set_watchdog_timer(WATCHDOG_DISABLE);

   Status = Pxe->Mtftp(Pxe, EFI_PXE_BASE_CODE_TFTP_READ_FILE,
                       Data, FALSE, &Size64, &tftp_block_size,
                       &ServerIp, (UINT8 *)filepath, NULL, FALSE);

  efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   if (EFI_ERROR(Status)) {
      free(Data);
      return Status;
   }

   /*
    * XXX: Use EFI_PXE_BASE_CODE_CALLBACK to call the progress callback each
    * time a packet is received.
    */
   if (callback != NULL) {
      error = callback((size_t)Size64);
      if (error != 0) {
         free(Data);
         return error_generic_to_efi(error);
      }
   }

   *Buffer = Data;
   *BufSize = (UINTN)Size64;

   return EFI_SUCCESS;
}
