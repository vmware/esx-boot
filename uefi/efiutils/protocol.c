/*******************************************************************************
 * Copyright (c) 2008-2011,2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * protocol.c -- Protocol and Handle management
 */

#include "efi_private.h"

/*-- get_protocol_interface ----------------------------------------------------
 *
 *      Queries an EFI handle to determine if it supports Protocol. If it does,
 *      then on return Interface points to a pointer to the corresponding
 *      Protocol interface.
 *
 * Parameters
 *      IN  Handle:    EFI handle to query
 *      IN  Protocol:  Protocol GUID
 *      OUT Interface: pointer to the protocol interface
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_protocol_interface(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                  void **Interface)
{
   EFI_STATUS Status;
   VOID *IFace;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->HandleProtocol != NULL);
   EFI_ASSERT_PARAM(Protocol != NULL);
   EFI_ASSERT_PARAM(Interface != NULL);

   Status = bs->HandleProtocol(Handle, Protocol, &IFace);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   if (IFace == NULL) {
      return EFI_NOT_FOUND;
   }

   *Interface = IFace;

   return EFI_SUCCESS;
}

/*-- LocateHandleByProtocol ----------------------------------------------------
 *
 *      Locate all the devices that support Protocol, and return their handles.
 *
 * Parameters
 *      IN  Protocol: Protocol GUID
 *      OUT count:    number of handles that were located
 *      OUT Handles:  pointer to the array of handles that were located
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS LocateHandleByProtocol(EFI_GUID *Protocol, UINTN *count,
                                  EFI_HANDLE **Handles)
{
   UINTN buflen = 64 * sizeof (EFI_HANDLE);
   EFI_HANDLE *buffer = NULL;
   EFI_STATUS Status;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->LocateHandle != NULL);
   EFI_ASSERT_PARAM(Protocol != NULL);
   EFI_ASSERT_PARAM(count != NULL);
   EFI_ASSERT_PARAM(Handles != NULL);

   do {
      if (buffer != NULL) {
         sys_free(buffer);
      }

      buffer = sys_malloc(buflen);
      if (buffer == NULL) {
         return EFI_OUT_OF_RESOURCES;
      }

      Status = bs->LocateHandle(ByProtocol, Protocol, NULL, &buflen, buffer);
      if (!EFI_ERROR(Status)) {
         *Handles = buffer;
         *count = buflen / sizeof (EFI_HANDLE);
         return EFI_SUCCESS;
      }
   } while (Status == EFI_BUFFER_TOO_SMALL);

   sys_free(buffer);
   return Status;
}

/*-- LocateProtocol ------------------------------------------------------------
 *
 *      Find the first device handle that support Protocol, and returns a
 *      pointer to the protocol interface from that handle.
 *
 *      NOTE: we are not using the LocateProtocol() Boot Service, because it is
 *      not available on EFI 1.02. Also, gnu-efi headers do not declare
 *      LocateProtocol() has a member of the EFI_BOOT_SERVICES structure.
 *
 *      XXX: we should check the firmware version, and use the LocateProtocol()
 *           Boot Service when it is >= EFI 1.10.
 *
 * Parameters
 *      IN  Protocol:  Protocol GUID
 *      OUT Interface: pointer to the protocol interface
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS LocateProtocol(EFI_GUID *Protocol, void **Interface)
{
   EFI_HANDLE *Handles;
   UINTN count, i;
   EFI_STATUS Status;

   Status = LocateHandleByProtocol(Protocol, &count, &Handles);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = EFI_UNSUPPORTED;

   for (i = 0; i < count; i++) {
      Status = get_protocol_interface(Handles[i], Protocol, Interface);
      if (!EFI_ERROR(Status)) {
         break;
      }
   }

   sys_free(Handles);
   return Status;
}
