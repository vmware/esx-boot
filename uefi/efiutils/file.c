/*******************************************************************************
 * Copyright (c) 2008-2016,2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * file.c -- EFI file access support
 *
 * Known file (and URL) access methods:
 * 1. gPXE download protocol
 * 2. HTTP
 * 3. Simple File Protocol
 * 4. Load File Protocol (NetBoot, or re-export of HTTP)
 * 5. TFTP (PXE boot)
 */

#include <string.h>
#include "efi_private.h"

typedef struct {
   EFI_STATUS (*load)(EFI_HANDLE Volume, const char *filepath,
                           int (*callback)(size_t), VOID **Buffer,
                           UINTN *BufSize);
   EFI_STATUS (*get_size)(EFI_HANDLE Volume, const char *filepath,
                          UINTN *FileSize);
   const char *name;
} file_access_methods;

/*-- unsupported ---------------------------------------------------------------
 *
 *      Placeholder for unsupported methods.
 *
 * Results
 *      EFI_UNSUPPORTED
 *----------------------------------------------------------------------------*/
static EFI_STATUS unsupported(void)
{
   return EFI_UNSUPPORTED;
}

static file_access_methods fam[] = {
   { gpxe_file_load, (void *)unsupported, "gpxe" },
   { http_file_load, http_file_get_size, "http" },
   { simple_file_load, simple_file_get_size, "simple" },
   { load_file_load, load_file_get_size, "load" },
   { tftp_file_load, tftp_file_get_size, "tftp" },
};

/*-- filepath_unix_to_efi ------------------------------------------------------
 *
 *      Convert a UNIX-style path to an equivalent EFI Path Name.
 *        - all occurrences of '/' are replaced with '\\'
 *        - the ASCII input is converted to UTF16
 *
 * Parameters
 *      IN  unix_path: pointer to the UNIX path
 *      OUT uefi_path: pointer to the freshly allocated UEFI Path Name
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS filepath_unix_to_efi(const char *unix_path,
                                CHAR16 **uefi_path)
{
   CHAR16 *Path;
   int i;
   EFI_STATUS Status;

   Path = NULL;
   Status = ascii_to_ucs2(unix_path, &Path);
   if (EFI_ERROR(Status)) {
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

/*-- firmware_file_read --------------------------------------------------------
 *
 *      Read a file.
 *
 * Parameters
 *      IN  filepath: absolute path to the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      OUT buffer:   pointer to where the file was loaded
 *      OUT buflen:   number of bytes that have been written into buffer
 *
 * Results
 *      EFI_SUCCESS, or an generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_read(const char *filepath,
                       int (*callback)(size_t),
                       void **buffer, size_t *buflen)
{
   EFI_STATUS Status;
   EFI_HANDLE Volume;
   unsigned try;

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   *buffer = NULL; // ensure a new buffer is allocated
   Status = EFI_UNSUPPORTED;

   /* Try each known file access method until one succeeds or all fail. */
   for (try = 0; try < ARRAYSIZE(fam); try++) {
      EFI_STATUS St;

      St = fam[try].load(Volume, filepath, callback, buffer, buflen);
      if (St != EFI_UNSUPPORTED && St != EFI_INVALID_PARAMETER) {
         Status = St;
      }
      if (!EFI_ERROR(St) || St == EFI_ABORTED) {
         break;
      }
   }

   if (!EFI_ERROR(Status)) {
      Log(LOG_DEBUG, "%s loaded via %s_file_load at %p, size %zu",
          filepath, fam[try].name, *buffer, *buflen);
   }

   return error_efi_to_generic(Status);
}

/*-- firmware_file_get_size_hint -----------------------------------------------
 *
 *      Try to get the size of a file.
 *
 *      In some circumstances, it may not be possible to get the size of the
 *      file without loading the full contents. This function is intended to be
 *      quick and non-authoritative, and will avoid downloading the file data
 *      if possible. If the size cannot be determined quickly, this function
 *      will return ERR_UNSUPPORTED.
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
   unsigned try;
   EFI_STATUS Status;

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Status = EFI_UNSUPPORTED;

   /* Try each known file access method until one succeeds or all fail. */
   for (try = 0; try < ARRAYSIZE(fam); try++) {
      EFI_STATUS St;

      St = fam[try].get_size(Volume, filepath, &size);
      if (St != EFI_UNSUPPORTED && St != EFI_INVALID_PARAMETER) {
         Status = St;
      }
      if (!EFI_ERROR(St) || St == EFI_ABORTED) {
         break;
      }
   }

   if (!EFI_ERROR(Status)) {
      *filesize = size;
   }

   return error_efi_to_generic(Status);
}

/*-- firmware_file_exec --------------------------------------------------------
 *
 *      Execute an UEFI binary (works for both application and driver).
 *      Works only for files on disk.
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
