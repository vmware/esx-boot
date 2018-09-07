/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * error.c -- EFI error handling
 */

#include <error.h>
#include "efi_private.h"

#define EFI_UNDEFINED_ERROR   EFI_SUCCESS
#define EFI_ERROR_MASK        MAX_BIT

static const EFI_STATUS efi_statuses[ERROR_NUMBER] = {
   EFI_SUCCESS,            EFI_UNDEFINED_ERROR,  EFI_LOAD_ERROR,
   EFI_INVALID_PARAMETER,  EFI_UNSUPPORTED,      EFI_BAD_BUFFER_SIZE,
   EFI_BUFFER_TOO_SMALL,   EFI_NOT_READY,        EFI_DEVICE_ERROR,
   EFI_WRITE_PROTECTED,    EFI_OUT_OF_RESOURCES, EFI_VOLUME_CORRUPTED,
   EFI_VOLUME_FULL,        EFI_NO_MEDIA,         EFI_MEDIA_CHANGED,
   EFI_NOT_FOUND,          EFI_ACCESS_DENIED,    EFI_NO_RESPONSE,
   EFI_NO_MAPPING,         EFI_TIMEOUT,          EFI_NOT_STARTED,
   EFI_ALREADY_STARTED,    EFI_ABORTED,          EFI_ICMP_ERROR,
   EFI_TFTP_ERROR,         EFI_PROTOCOL_ERROR,   EFI_INCOMPATIBLE_VERSION,
   EFI_SECURITY_VIOLATION, EFI_CRC_ERROR,        EFI_END_OF_MEDIA,
   EFI_END_OF_FILE,        EFI_INVALID_LANGUAGE, EFI_UNDEFINED_ERROR,
   EFI_UNDEFINED_ERROR,    EFI_UNDEFINED_ERROR,  EFI_UNDEFINED_ERROR,
   EFI_UNDEFINED_ERROR,    EFI_UNDEFINED_ERROR,  EFI_UNDEFINED_ERROR
};

/*-- error_efi_to_generic ------------------------------------------------------
 *
 *      Convert an UEFI error status to a generic error value.
 *
 * Parameters
 *      IN Status: EFI error status
 *
 * Results
 *      Equivalent generic error value.
 *----------------------------------------------------------------------------*/
int error_efi_to_generic(EFI_STATUS Status)
{
   int i;

   if (!(Status & EFI_ERROR_MASK)) {
      switch (Status) {
         case EFI_SUCCESS:
            break;
         case EFI_WARN_DELETE_FAILURE:
         case EFI_WARN_WRITE_FAILURE:
            Status = EFI_DEVICE_ERROR;
            break;
         case EFI_WARN_BUFFER_TOO_SMALL:
            Status = EFI_BUFFER_TOO_SMALL;
            break;
         case EFI_WARN_UNKNOWN_GLYPH:
         default:
            /* Not supposed to happen, but just in case... */
            return ERR_UNKNOWN;
      }
   }

   for (i = 0; i < ERROR_NUMBER; i++) {
      if (Status == efi_statuses[i]) {
         return i;
      }
   }

   return ERR_UNKNOWN;
}

/*-- error_generic_to_efi ------------------------------------------------------
 *
 *      Convert a generic error value to an equivalent UEFI status. If no
 *      equivalent exists for the given error, then EFI_ABORTED is returned.
 *
 * Parameters
 *      IN err: generic error value
 *
 * Results
 *      The equivalent UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS error_generic_to_efi(int err)
{
   if (err < ERROR_NUMBER) {
      return efi_statuses[err];
   }

   return EFI_ABORTED;
}
