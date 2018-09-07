/*******************************************************************************
 * Copyright (c) 2008-2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/* file.c -- EFI file access support
 *
 *   Conventions
 *
 *    - Functions below must be passed absolute pathnames.
 *    - A NULL label indicates that the file will be found on the boot volume.
 */

#include <string.h>
#include "efi_private.h"

/*-- filepath_unix_to_efi ------------------------------------------------------
 *
 *      Convert a UNIX-style path to an equivalent EFI Path Name.
 *        - all occurrences of '/' are replaced with '\\'
 *        - double-separator "\\" occurences are merged
 *        - the ASCII input is converted to UTF16
 *
 * Parameters
 *      IN  unix_path: pointer to the UNIX path
 *      OUT uefi_path: pointer to the freshly allocated UEFI Path Name
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS filepath_unix_to_efi(const char *unix_path,
                                       CHAR16 **uefi_path)
{
   CHAR16 *Path;
   char *filepath;
   int i, status;
   EFI_STATUS Status;

   Path = NULL;

   filepath = strdup(unix_path);
   if (filepath == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   status = file_sanitize_path(filepath);
   if (status != ERR_SUCCESS) {
      sys_free(filepath);
      return status;
   }

   Status = ascii_to_ucs2(filepath, &Path);
   if (EFI_ERROR(Status)) {
      sys_free(filepath);
      return Status;
   }

   for (i = 0; Path[i] != L'\0'; i++) {
      if (Path[i] == '/') {
         Path[i] = L'\\';
      }
   }

   *uefi_path = Path;

   return EFI_SUCCESS;
}

/*-- filepath_efi_to_ascii -----------------------------------------------------
 *
 *      Convert a formatted UEFI path to an ascii string. Every occurrence of a
 *      '\\' is replaced with a '/'
 *
 * Parameters
 *      IN  EfiPath:    pointer to the UEFI path UCS-2 string
 *      OUT ascii_path: pointer to the freshly allocated ascii path
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS filepath_efi_to_ascii(const CHAR16 *EfiPath,
                                        char **ascii_path)
{
   char *ascii;
   EFI_STATUS Status;

   ascii = NULL;
   Status = ucs2_to_ascii(EfiPath, &ascii, false);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   *ascii_path = ascii;

   while (*ascii != '\0') {
      if (*ascii == '\\') {
         *ascii = '/';
      }
      ascii++;
   }

   return EFI_SUCCESS;
}

/*-- efi_file_read -------------------------------------------------------------
 *
 *      Read a file.
 *        1. First try using the gPXE download protocol.
 *        2. Next, try using the Simple File Protocol.
 *        2. Next, try using the Load File Protocol (Netboot).
 *        3. Finally, try using TFTP (PXE boot).
 *
 * Parameters
 *      IN  Volume:   handle to the volume from which to load the file
 *      IN  FilePath: absolute path to the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      OUT buffer:   pointer to where the file was loaded
 *      OUT buflen:   number of bytes that have been written into buffer
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS efi_file_read(EFI_HANDLE Volume, const CHAR16 *FilePath,
                         int (*callback)(size_t), VOID **Buffer, UINTN *BufLen)
{
   char *ascii_path;
   int try;
   EFI_STATUS Status, St;

   ascii_path = NULL;
   Status = filepath_efi_to_ascii(FilePath, &ascii_path);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = EFI_UNSUPPORTED;
   for (try = 0; try < 4; try++) {
      if (try == 0) {
         St = gpxe_file_load(Volume, ascii_path, callback, Buffer, BufLen);
      } else if (try == 1) {
         St = simple_file_load(Volume, FilePath, callback, Buffer, BufLen);
      } else if (try == 2) {
         St = load_file_load(Volume, FilePath, callback, Buffer, BufLen);
      } else {
         St = tftp_file_load(Volume, ascii_path, callback, Buffer, BufLen);
      }

      if (St != EFI_UNSUPPORTED) {
         Status = St;
      }
      if (!EFI_ERROR(St) || (St == EFI_ABORTED)) {
         break;
      }
   }

   if (!EFI_ERROR(St)) {
      static const char *method[] = { "gpxe_file_load", "simple_file_load",
                                      "load_file_load", "tftp_file_load" };
      Log(LOG_DEBUG, "%s loaded via %s", ascii_path, method[try]);
   }

   sys_free(ascii_path);

   return Status;
}

/*-- firmware_file_read --------------------------------------------------------
 *
 *      Read a file from the boot volume.
 *
 * Parameters
 *      IN  filepath: absolute path to the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      OUT buffer:   pointer to where the file was loaded
 *      OUT buflen:   number of bytes that have been written into buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_read(const char *filepath, int (*callback)(size_t),
                       void **buffer, size_t *buflen)
{
   EFI_HANDLE Volume;
   CHAR16 *Path;
   EFI_STATUS Status;

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Status = filepath_unix_to_efi(filepath, &Path);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Status = efi_file_read(Volume, Path, callback, buffer, buflen);

   sys_free(Path);

   return error_efi_to_generic(Status);
}

/*-- firmware_file_get_size_hint -----------------------------------------------
 *
 *      Try to get the size of a file.
 *        1. First try using the Simple File Protocol.
 *        2. Next, try using the Load File Protocol (Netboot).
 *        3. Finally, try using TFTP (PXE boot).
 *
 *      In some circumstances, it may not be possible to get the size of the
 *      file without loading the full contents. This function is intended to be
 *      quick and non-authoritative, and will avoid downloading the file data
 *      if possible. If the size cannot be determined quickly, this function
 *      will return ERR_UNSUPPORTED.
 *
 *      NOTE: The reason we don't see gPXE protocol here is because, gPXE
 *      doesn't support a method to obtain the size of a given file without
 *      first downloading the entire file.
 *
 * Parameters
 *      IN  filepath: absolute path to the file
 *      OUT filesize: the file size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_get_size_hint(const char *filepath, size_t *filesize)
{
   EFI_HANDLE Volume;
   UINTN size;
   CHAR16 *Path;
   int try;
   EFI_STATUS Status;

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Status = filepath_unix_to_efi(filepath, &Path);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   for (try = 0; try < 3; try++) {
      if (try == 0) {
         Status = simple_file_get_size(Volume, Path, &size);
      } else if (try == 1) {
         Status = load_file_get_size(Volume, Path, &size);
      } else {
         Status = tftp_file_get_size(Volume, filepath, &size);
      }

      if (!EFI_ERROR(Status)) {
         *filesize = size;
         break;
      } else if (Status != EFI_UNSUPPORTED &&
                 Status != EFI_INVALID_PARAMETER) {
         break;
      }
   }

   sys_free(Path);
   return error_efi_to_generic(Status);
}

/*-- firmware_file_exec --------------------------------------------------------
 *
 *      Execute an UEFI binary (works for both application and driver).
 *
 * Parameters
 *      IN filepath: absolute path to the file
 *      IN options:  pointer to the command line options
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_exec(const char *filepath, const char *options)
{
   CHAR16 *optbuf, *Path;
   EFI_HANDLE Volume;
   EFI_STATUS Status;

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Status = filepath_unix_to_efi(filepath, &Path);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (options != NULL) {
      options = strchr(options, ' ');
   }
   options = (options == NULL) ? "" : options + 1;

   optbuf = NULL;
   Status = ascii_to_ucs2(options, &optbuf);
   if (EFI_ERROR(Status)) {
      sys_free(Path);
      return error_efi_to_generic(Status);
   }

   Status = image_load(Volume, Path, optbuf, (UINT32)UCS2SIZE(optbuf),
                       NULL, NULL);

   sys_free(optbuf);
   sys_free(Path);

   return error_efi_to_generic(Status);
}
