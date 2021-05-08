/*******************************************************************************
 * Copyright (c) 2008-2011,2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * protocoll.c -- Protocol and Handle management
 */

#include "efi_private.h"

/*-- log_protocols_on_handle ---------------------------------------------------
 *
 *      Log the GUID of each protocol that is installed on the given handle.
 *
 * Parameters
 *      IN  level:  log level
 *      IN  label:  human readable name for the handle
 *      IN  Handle: handle
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
void log_protocols_on_handle(int level, const char *label, EFI_HANDLE Handle)
{
   EFI_STATUS Status;
   EFI_GUID **ProtocolBuffer;
   UINTN ProtocolBufferCount, i;

   Log(level, "Protocol GUIDs on handle %p (%s):", Handle, label);

   if (Handle == NULL) {
      return;
   }
   Status = bs->ProtocolsPerHandle(Handle, &ProtocolBuffer,
                                   &ProtocolBufferCount);
   if (EFI_ERROR(Status)) {
      Log(level, "Error in ProtocolsPerHandle: %s",
          error_str[error_efi_to_generic(Status)]);
      return;
   }

   for (i = 0; i < ProtocolBufferCount; i++) {
      EFI_GUID *g = ProtocolBuffer[i];
      Log(level, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          g->Data1, g->Data2, g->Data3,
          g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
          g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
   }
   if (ProtocolBufferCount > 0) {
      sys_free(ProtocolBuffer);
   }
}

