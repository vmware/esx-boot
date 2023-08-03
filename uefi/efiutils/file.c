/*******************************************************************************
 * Copyright (c) 2008-2016,2019-2022 VMware, Inc.  All rights reserved.
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
   EFI_STATUS (*save)(EFI_HANDLE Volume, const char *filepath,
                           int (*callback)(size_t), VOID *Buffer,
                           UINTN BufSize);
   EFI_STATUS (*get_size)(EFI_HANDLE Volume, const char *filepath,
                          UINTN *FileSize);
   const char *name;
} file_access_methods;

static file_access_methods *last_fam = NULL;

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
   { gpxe_file_load, (void *)unsupported, (void *)unsupported, "gpxe" },
   { http_file_load, (void *)unsupported, http_file_get_size, "http" },
   { simple_file_load, simple_file_save, simple_file_get_size, "simple" },
   { load_file_load, (void *)unsupported, load_file_get_size, "load" },
   { tftp_file_load, (void *)unsupported, tftp_file_get_size, "tftp" },
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
#if DEBUG
      Log(LOG_DEBUG, "%s_file_load returns %s", fam[try].name,
          error_str[error_efi_to_generic(St)]);
#endif
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
      last_fam = &fam[try];
   }

   return error_efi_to_generic(Status);
}

/*-- last_file_read_via_http ---------------------------------------------------
 *
 *      Was the last successful file read via native UEFI http?
 *
 * Results
 *      bool
 *----------------------------------------------------------------------------*/
bool last_file_read_via_http(void)
{
   return last_fam != NULL && strcmp(last_fam->name, "http") == 0;
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

/*-- firmware_image_load -------------------------------------------------------
 *
 *      Load a UEFI image that is already in memory, but don't start it.  Works
 *      for both application and driver.
 *
 * Parameters
 *      IN filepath:     absolute path the binary was loaded from
 *      IN options:      pointer to the command line options
 *      IN image:        pointer to the binary image
 *      IN imgsize:      size of the image
 *      OUT ChildHandle: child handle
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_image_load(const char *filepath, const char *options,
                        void *image, size_t imgsize,
                        EFI_HANDLE *ChildHandle)
{
   EFI_STATUS Status;
   EFI_HANDLE Volume;
   EFI_DEVICE_PATH *ChildPath;
   EFI_HANDLE ChildDH;
   EFI_LOADED_IMAGE *Child;
   CHAR16 *LoadOptions = NULL;

   *ChildHandle = NULL;

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (options == NULL) {
      options = "";
   }
   Status = ascii_to_ucs2(options, &LoadOptions);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   /*
    * Compute values to pass in the DevicePath argument to LoadImage
    * (ChildPath) and in (*ChildHandle)->DeviceHandle (ChildDH).
    */
   if (strstr(filepath, "://") != NULL && last_file_read_via_http()) {
      Status = make_http_child_dh(Volume, filepath, &ChildDH);
      if (EFI_ERROR(Status)) {
         goto out;
      }
      Status = devpath_get(ChildDH, &ChildPath);
      if (EFI_ERROR(Status)) {
         goto out;
      }
   } else {
      CHAR16 *Filepath = NULL;
      Status = ascii_to_ucs2(filepath, &Filepath);
      if (EFI_ERROR(Status)) {
         goto out;
      }
      Status = file_devpath(Volume, Filepath, &ChildPath);
      sys_free(Filepath);
      if (EFI_ERROR(Status)) {
         goto out;
      }
      ChildDH = Volume;
   }

   /* Use the form of LoadImage that takes a memory buffer */
   Status = bs->LoadImage(FALSE, ImageHandle, ChildPath,
                          image, imgsize, ChildHandle);
   if (EFI_ERROR(Status)) {
      goto out;
   }

   /* Pass the command line, system table, and boot volume to the child */
   Status = image_get_info(*ChildHandle, &Child);
   if (EFI_ERROR(Status)) {
      goto out;
   }

   Child->LoadOptions = LoadOptions;
   Child->LoadOptionsSize = UCS2SIZE(LoadOptions);
   Child->SystemTable = st;
   Child->DeviceHandle = ChildDH;

   Log(LOG_DEBUG, "Image %s loaded at %p (size 0x%"PRIx64")",
       filepath, Child->ImageBase, Child->ImageSize);

 out:
   if (EFI_ERROR(Status)) {
      if (*ChildHandle != NULL) {
         bs->UnloadImage(*ChildHandle);
         *ChildHandle = NULL;
      }
      sys_free(LoadOptions);
   }
   return error_efi_to_generic(Status);
}

/*-- firmware_image_start ------------------------------------------------------
 *
 *      Start a child image that has been loaded.  This function may or may not
 *      return.  It will return if there is an error, if the child is an app
 *      that exits and is unloaded, or if the child is a driver that finishes
 *      initialization and remains resident.
 *
 *      Note: If this function returns an error, you can't tell in general
 *      whether the child could not be started, or the child was started and
 *      ran but returned an error status.
 *
 * Parameters
 *      IN handle:   child handle
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_image_start(EFI_HANDLE ChildHandle)
{
   EFI_STATUS Status;
   UINTN ExitDataSize;
   CHAR16 *ExitData = NULL;
   int status;

   firmware_reset_watchdog();
   Status = bs->StartImage(ChildHandle, &ExitDataSize, &ExitData);
   status = error_efi_to_generic(Status);
   if (EFI_ERROR(Status)) {
      char *exit_data = NULL;
      if (ExitData != NULL) {
         ucs2_to_ascii(ExitData, &exit_data, FALSE);
      }
      Log(LOG_WARNING, "StartImage returned %s, %s",
          error_str[status], exit_data);
      sys_free(exit_data);
   }
   sys_free(ExitData);

   return status;
}

/*-- firmware_filepath_load --------------------------------------------------
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
static int
firmware_filepath_load(const char *filepath, const char *options)
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

/*-- firmware_file_exec --------------------------------------------------------
 *
 *      Execute a UEFI binary.  Works for both application and driver, and for
 *      any file that firmware_file_read can load.
 *
 *      Future: We could merge image_load() with this function.  Each function
 *      currently provides features the other one lacks.
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
   int status;
   void *image;
   size_t imgsize;
   EFI_HANDLE ChildHandle;

   status = firmware_file_read(filepath, NULL, &image, &imgsize);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = firmware_image_load(filepath, options, image, imgsize,
                                &ChildHandle);
   if (status == ERR_SUCCESS) {
      return firmware_image_start(ChildHandle);
   }

   /*
    * Loading an image copied to memory failed; attempt with the file path
    * instead.
    */
   sys_free(image);
   return firmware_filepath_load(filepath, options);
}


/*-- firmware_file_write -------------------------------------------------------
 *
 *      Write an entire file.
 *
 * Parameters
 *      IN  filepath: absolute path of the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    written
 *      IN  buffer:   pointer to buffer being written
 *      IN  bufsize:  number of bytes to write
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_write(const char *filepath, int (*callback)(size_t),
                        void *buffer, size_t bufsize)
{
   EFI_STATUS Status;
   EFI_HANDLE Volume;
   unsigned try;

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Status = EFI_UNSUPPORTED;

   /* Try each known file access method until one succeeds or all fail. */
   for (try = 0; try < ARRAYSIZE(fam); try++) {
      EFI_STATUS St;

      St = fam[try].save(Volume, filepath, callback, buffer, bufsize);
#if DEBUG
      Log(LOG_DEBUG, "%s_file_save returns %s", fam[try].name,
          error_str[error_efi_to_generic(St)]);
#endif
      if (St != EFI_UNSUPPORTED && St != EFI_INVALID_PARAMETER) {
         Status = St;
      }
      if (!EFI_ERROR(St) || St == EFI_ABORTED) {
         break;
      }
   }

   if (!EFI_ERROR(Status)) {
      Log(LOG_DEBUG, "%s saved via %s_file_save at %p, size %zu",
          filepath, fam[try].name, buffer, bufsize);
      last_fam = &fam[try];
   }

   return error_efi_to_generic(Status);
}
