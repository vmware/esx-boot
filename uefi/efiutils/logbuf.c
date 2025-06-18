/*******************************************************************************
 * Copyright (c) 2023 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * logbuf.c -- EFI-specific log buffer functions.
 */

#include "efi_private.h"
#include <efiutils.h>
#include <logbuf.h>

// Log buffer protocol interface.

static VMW_LOGBUFFER_PROTOCOL LogBufProto = {
    LOGBUF_CURRENT_API_VERSION,
    "VMware's ESXboot Log Library, v1.0",
    NULL,
};

/*-- logbuf_proto_init ---------------------------------------------------------
 *
 *      Installs the UEFI protocol for log buffer interface. Also captures the
 *      location of the syslogbuf struct.
 *
 * Parameters
 *      IN syslogbuf: pointer to log buffer information structure.
 *
 * Results
 *      Generic error status.
 *----------------------------------------------------------------------------*/
int logbuf_proto_init(struct syslogbuffer *syslogbuf)
{
   EFI_HANDLE ImageHandle = NULL;
   EFI_GUID LogBufProtoID = EFI_LOG_PROTOCOL_GUID;
   EFI_STATUS Status;

   LogBufProto.syslogbuf = syslogbuf;
   Status = bs->InstallProtocolInterface(&ImageHandle,
                                         &LogBufProtoID,
                                         EFI_NATIVE_INTERFACE,
                                         &LogBufProto);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   return ERR_SUCCESS;
}

/*-- logbuf_proto_get ----------------------------------------------------------
 *
 *      Protocol interface to get the log buffer details.
 *
 * Parameters
 *      OUT syslogbuf: pointer to log buffer information structure.
 *
 * Results
 *      Generic error status.
 *----------------------------------------------------------------------------*/
int logbuf_proto_get(struct syslogbuffer **syslogbuf)
{
   EFI_GUID LogBufProtoID = EFI_LOG_PROTOCOL_GUID;
   VMW_LOGBUFFER_PROTOCOL *logBufProto = NULL;
   EFI_STATUS Status;

   Status = LocateProtocol(&LogBufProtoID, (void **) &logBufProto);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (logBufProto->ApiVersion != LOGBUF_CURRENT_API_VERSION) {
      Log(LOG_DEBUG, "Got log buffer protocol API version %u; expected %u",
          logBufProto->ApiVersion, LOGBUF_CURRENT_API_VERSION);
      return ERR_INCOMPATIBLE_VERSION;
   }

   *syslogbuf = logBufProto->syslogbuf;
   return ERR_SUCCESS;
}
