/*******************************************************************************
 * Copyright (c) 2008-2011,2019-2020,2022 VMware, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * error.c -- EFI error handling
 */

#include <error.h>
#include "efi_private.h"

#define EFI_ERROR_MASK        MAX_BIT

#define D(symbol, efi_symbol, string) efi_symbol,
static const EFI_STATUS efi_statuses[] = {
   ERROR_TABLE
};
#undef D

/*-- error_efi_to_generic ------------------------------------------------------
 *
 *      Convert a UEFI error status to a generic error value.
 *
 *      Note: If a UEFI function can return a warning status, its caller is
 *      expected to specifically handle or ignore it, not blindly pass it to
 *      this function.  If a UEFI warning status is passed in, this function
 *      returns ERR_SUCCESS.
 *
 * Parameters
 *      IN Status: EFI error status
 *
 * Results
 *      Equivalent generic error value if known, otherwise ERR_UNKNOWN.
 *----------------------------------------------------------------------------*/
int error_efi_to_generic(EFI_STATUS Status)
{
   int i;

   if (!EFI_ERROR(Status)) {
      return ERR_SUCCESS;
   }

   /*
    * Start after ERR_SUCCESS (handled above) and ERR_UNKNOWN, to
    * avoid translating EFI_INVALID_PARAMETER to EFI_UNKNOWN.
    */
   for (i = ERR_UNKNOWN + 1; i < ERROR_NUMBER; i++) {
      if (Status == efi_statuses[i]) {
         return i;
      }
   }

   return ERR_UNKNOWN;
}

/*-- error_generic_to_efi ------------------------------------------------------
 *
 *      Convert a generic error value to an equivalent UEFI status. If
 *      no equivalent exists for the given error, then EFI_INVALID_PARAMETER
 *      is returned.
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

   return EFI_UNDEFINED_ERROR;
}
