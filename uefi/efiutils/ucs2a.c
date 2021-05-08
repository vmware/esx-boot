/*******************************************************************************
 * Copyright (c) 2008-2011,2013-2016,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include <ctype.h>
#include <string.h>
#include "efi_private.h"

/*-- argv_to_ucs2 --------------------------------------------------------------
 *
 *      Convert an argv-like array into a UCS-2 single string.
 *
 * Parameters
 *      IN  argc:   Number of elements in argv like array
 *      IN  argv:   Pointer to the command line list of arguments.
 *      OUT ArgStr: Pointer to the freshly allocated UCS-2 string.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS argv_to_ucs2(int argc, char **argv, CHAR16 **ArgStr)
{
   EFI_STATUS Status;
   CHAR16 *Str;
   int status;
   char *str;

   if (ArgStr == NULL) {
      return EFI_INVALID_PARAMETER;
   }
   status = argv_to_str(argc, argv, &str);
   Status = error_generic_to_efi(status);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Str = NULL;
   Status = ascii_to_ucs2(str, &Str);
   if (EFI_ERROR(Status)) {
      sys_free(str);
      return Status;
   }

   sys_free(str);
   *ArgStr = Str;

   return EFI_SUCCESS;
}
