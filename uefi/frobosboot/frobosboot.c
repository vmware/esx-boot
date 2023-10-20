/*******************************************************************************
 * Copyright (c) 2015-2017,2022-2023 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * frobosboot.c --
 *
 *       Block device chainloading for frobos.
 */

/*
 * frobosboot is used for chainboot frobos tests with UEFI natively.
 * Before chain loading mboot, it reads a non-volatile counter to
 * determine which test to boot, displays test list and provides ways
 * to select test.
 *
 * People can use Up/Down, PageUp/PageDown to review test, and there's
 * two ways to select test, one is press 'Enter' on the test where the
 * cursor points to, the other way is enter test number directly.
 */

#include "frobosboot.h"

/*
 * This naming convention is unique to frobos and is
 * unrelated to ESXi.
 */
#ifndef NEXT_LOADER
   #if defined(only_riscv64)
      #define NEXT_LOADER L"\\EFI\\BOOT\\MBOOTRISCV64.EFI"
   #elif defined(only_arm64)
      #define NEXT_LOADER L"\\EFI\\BOOT\\MBOOTAA64.EFI"
   #elif defined(only_em64t)
      #define NEXT_LOADER L"\\EFI\\BOOT\\MBOOTx64.EFI"
   #else
      #define NEXT_LOADER L"\\EFI\\BOOT\\MBOOTIA32.EFI"
   #endif
#endif

/*-- filepath_unix_to_uefi -----------------------------------------------------
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
EFI_STATUS filepath_unix_to_uefi(const char *unix_path, CHAR16 **uefi_path)
{
   CHAR16 *Path, *p;
   bool sep;
   int i;
   EFI_STATUS Status;

   Path = NULL;
   Status = ascii_to_ucs2(unix_path, &Path);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   sep = false;
   p = Path;

   for (i = 0; Path[i] != L'\0'; i++) {
      if (IS_PATH_SEPARATOR(Path[i])) {
         if (sep == 0) {
            *p++ = L'\\';
            sep = true;
         }
      } else {
         *p++ = Path[i];
         sep = false;
      }
   }

   *p = L'\0';

   *uefi_path = Path;

   return EFI_SUCCESS;
}

/*-- connect_all_controllers ---------------------------------------------------
 *
 *      Connect all drivers to all controllers.
 *
 *      The firmware may have only connected drivers to a few devices. Attempt
 *      to recursively connect any drivers on the other un-connected devices.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS connect_all_controllers(void)
{
   EFI_HANDLE *handles;
   EFI_STATUS Status;
   UINTN i, n;

   Status = bs->LocateHandleBuffer(AllHandles, NULL, NULL, &n, &handles);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   for (i = 0; i < n; i++) {
      bs->ConnectController(handles[i], NULL, NULL, TRUE);
   }

   sys_free(handles);

   return ERR_SUCCESS;
}

int main(int argc, char **argv)
{
   EFI_STATUS Status, ChildStatus;
   EFI_HANDLE *handles;
   CHAR16 *FilePath;
   char bootconfig[BOOT_CFG_LEN];
   CHAR16 *LoadOptions;
   UINT32 LoadOptionsSize;
   UINTN i, n;
   int retval;
   char **tmpArgv;
   int tmpArgc;

   LoadOptions = NULL;
   LoadOptionsSize = 0;

   Status = connect_all_controllers();
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }
   Status = LocateHandleByProtocol(&BlockIoProto, &n, &handles);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (argc > 1) {
      Status = filepath_unix_to_uefi(argv[1], &FilePath);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }
   } else {
      FilePath = (CHAR16 *)NEXT_LOADER;
   }

   retval = frobos_get_next_boot(bootconfig);

   if (retval == EFI_SUCCESS) {
      /*
       * Because argv was allocated to be exactly argc elements long and
       * additional bootconfig arguments are needed to be added in, a
       * temporary argv is being created to accomodate for this.
       */
      tmpArgc = argc + 3;
      tmpArgv = sys_malloc(tmpArgc * sizeof *argv);
      if (tmpArgv == NULL) {
         return ERR_OUT_OF_RESOURCES;
      }
      memcpy(tmpArgv, argv, argc * sizeof *argv);
      tmpArgv[argc] = strdup("-c");
      tmpArgv[argc + 1] = strdup(bootconfig);
      tmpArgv[argc + 2] = NULL;
      /*
       * Get the Load Options, if any, to be passed to the next boot loader.
       * The first argument i.e. argv[0] has the executable name.
       */
      Status = argv_to_ucs2(tmpArgc - 1, tmpArgv + 1, &LoadOptions);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }

      LoadOptionsSize = (UINT32)UCS2SIZE(LoadOptions);
      /*
       * After getting the Load Options, the temporary argv is no longer needed
       * so it is now being freed. Only the added bootconfig options are being
       * freed here since efi_main will free the remainder of argv.
       */
      for (i = tmpArgc - 3; (int)i < tmpArgc; i++) {
         sys_free(tmpArgv[i]);
      }
      sys_free(tmpArgv);
   }

   for (i = 0; i < n; i++) {
      Status = image_load(handles[i], FilePath, LoadOptions, LoadOptionsSize,
                          NULL, &ChildStatus);
      if (!EFI_ERROR(Status)) {
         Status = ChildStatus;
         break;
      }
   }

   if (argc > 1) {
      bs->FreePool(FilePath);
      efi_free(LoadOptions);
   }
   bs->FreePool(handles);

   return error_efi_to_generic(Status);
}
