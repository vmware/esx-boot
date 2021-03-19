/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tpm.c -- TPM implementation for EFI.
 */

#include "efi_private.h"


/*-- tpm_get_event_log --------------------------------------------------------
 *
 *      Return a reference to the EFI memory location of the TPM event
 *      log. We only support the EFI_TCG2_EVENT_LOG_FORMAT_TCG_2 format,
 *      and an error is returned if that format is not available.
 *
 *      See TCG EFI Protocol Specification, Family “2.0”, Level 00
 *      Revision 00.13, March 30, 2016, Section 5: Event Log Structure
 *
 * Parameters
 *      IN log:  The TPM event log details.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int tpm_get_event_log(tpm_event_log_t *log)
{
   const uint8_t *address;
   uint32_t size;
   bool truncated;
   int status;

   status = tcg2_get_event_log(&address, &size, &truncated);
   if (status != EFI_SUCCESS) {
      Log(LOG_DEBUG, "TPM event log not available: %d", status);
      return ERR_NOT_FOUND;
   }

   log->address = address;
   log->size = size;
   log->truncated = truncated;

   Log(LOG_DEBUG, "TPM event log size: %u", size);
   return ERR_SUCCESS;
}
