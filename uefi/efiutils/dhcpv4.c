/*******************************************************************************
 * Copyright (c) 2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * dhcpv4.c -- Support for making a DHCPv4 request.
 */

#include "efi_private.h"
#include "ServiceBinding.h"
#include "Dhcp4.h"
#include "Ip4Config2.h"

static EFI_GUID Dhcp4ServiceBindingProto =
   EFI_DHCP4_SERVICE_BINDING_PROTOCOL_GUID;
static EFI_GUID Dhcp4Proto = EFI_DHCP4_PROTOCOL_GUID;
static EFI_GUID Ip4Config2Proto = EFI_IP4_CONFIG2_PROTOCOL_GUID;

/*
 * Cached information.
 */
static EFI_HANDLE Dhcp4NicHandle;
static EFI_SERVICE_BINDING_PROTOCOL *Dhcp4ServiceBinding;
static EFI_HANDLE Dhcp4Handle;
static EFI_DHCP4_PROTOCOL *Dhcp4;

/*-- has_ipv4_addr -------------------------------------------------------------
 *
 *      Check whether the given NIC has an IPv4 address.
 *
 * Parameters
 *      IN  NicHandle: Handle for NIC to use.
 *
 * Results
 *      EFI_SUCCESS (=has address) or error.
 *----------------------------------------------------------------------------*/
EFI_STATUS has_ipv4_addr(EFI_HANDLE NicHandle)
{
   EFI_STATUS Status;
   EFI_IP4_CONFIG2_PROTOCOL *Ip4Config2;
   UINTN DataSize;
   EFI_IPv4_ADDRESS Local;
   static EFI_IPv4_ADDRESS Zero = { .Addr = {0, 0, 0, 0} };
   EFI_IP4_CONFIG2_INTERFACE_INFO *Info;

   Status = get_protocol_interface(NicHandle, &Ip4Config2Proto,
                                   (void**)&Ip4Config2);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting Ip4Config2 protocol: %s",
          error_str[error_efi_to_generic(Status)]);
      return Status;
   }

   DataSize = 0;
   Status = Ip4Config2->GetData(Ip4Config2, Ip4Config2DataTypeInterfaceInfo,
                                &DataSize, NULL);
   if (Status != EFI_BUFFER_TOO_SMALL) {
      Log(LOG_ERR, "Error in Ip4Config2->GetData (info size): %s",
          error_str[error_efi_to_generic(Status)]);
      return Status;
   }
   Info = sys_malloc(DataSize);
   Status = Ip4Config2->GetData(Ip4Config2, Ip4Config2DataTypeInterfaceInfo,
                                &DataSize, Info);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error in Ip4Config2->GetData (info): %s",
          error_str[error_efi_to_generic(Status)]);
      return Status;
   }

   Local = Info->StationAddress;
   if (memcmp(&Local, &Zero, sizeof(Local)) != 0) {
      Log(LOG_DEBUG, "Existing local IPv4 address = %u.%u.%u.%u",
          Local.Addr[0], Local.Addr[1], Local.Addr[2], Local.Addr[3]);
   } else {
      Log(LOG_DEBUG, "No existing local IPv4 address");
      Status = EFI_NO_MAPPING;
   }

   sys_free(Info);
   return Status;
}

/*-- set_policy_dhcp4 ----------------------------------------------------------
 *
 *      Set IPv4 policy to DHCP.
 *
 * Parameters
 *      IN  NicHandle: Handle for NIC to use.
 *
 * Results
 *      EFI_SUCCESS or an error.
 *----------------------------------------------------------------------------*/
EFI_STATUS set_policy_dhcp4(EFI_HANDLE NicHandle)
{
   EFI_STATUS Status;
   EFI_IP4_CONFIG2_PROTOCOL *Ip4Config2;
   UINTN DataSize;
   EFI_IP4_CONFIG2_POLICY Policy;

   Status = get_protocol_interface(NicHandle, &Ip4Config2Proto,
                                   (void**)&Ip4Config2);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting Ip4Config2 protocol: %s",
          error_str[error_efi_to_generic(Status)]);
      return Status;
   }

   DataSize = sizeof(Policy);
   Status = Ip4Config2->GetData(Ip4Config2, Ip4Config2DataTypePolicy,
                                &DataSize, &Policy);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error in Ip4Config2->GetData (policy): %s",
          error_str[error_efi_to_generic(Status)]);
      return Status;
   }

   if (Policy == Ip4Config2PolicyDhcp) {
      return EFI_SUCCESS;
   }

   Log(LOG_DEBUG, "Changing IPv4 policy to DHCP");
   Policy = Ip4Config2PolicyDhcp;
   Status = Ip4Config2->SetData(Ip4Config2, Ip4Config2DataTypePolicy,
                                sizeof(Policy), &Policy);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error in Ip4Config2->SetData (policy): %s",
          error_str[error_efi_to_generic(Status)]);
      return Status;
   }

   return EFI_SUCCESS;
}

/*-- dhcp4_cleanup -------------------------------------------------------------
 *
 *      Clean up cached Dhcp4 instance.
 *----------------------------------------------------------------------------*/
void dhcp4_cleanup(void)
{
   if (Dhcp4 != NULL) {
      Dhcp4->Stop(Dhcp4);
      Dhcp4->Configure(Dhcp4, NULL);
      Dhcp4 = NULL;
   }
   if (Dhcp4Handle != NULL) {
      Dhcp4ServiceBinding->DestroyChild(Dhcp4ServiceBinding, Dhcp4Handle);
      Dhcp4Handle = NULL;
   }
   Dhcp4ServiceBinding = NULL;
   Dhcp4NicHandle = NULL;
}

/*-- run_dhcpv4 ----------------------------------------------------------------
 *
 *      Kick off DHCPv4 and wait for it to complete.
 *
 * Parameters
 *      IN  NicHandle: Handle for NIC to use.
 *
 * Results
 *      EFI_SUCCESS or an error.
 *----------------------------------------------------------------------------*/
EFI_STATUS run_dhcpv4(EFI_HANDLE NicHandle)
{
   EFI_STATUS Status;
   EFI_DHCP4_CONFIG_DATA Dhcp4CfgData;
   EFI_DHCP4_MODE_DATA Dhcp4ModeData;

   if (Dhcp4NicHandle != NULL && NicHandle != Dhcp4NicHandle) {
      dhcp4_cleanup();
   }

   if (Dhcp4ServiceBinding == NULL) {
      Status = get_protocol_interface(NicHandle, &Dhcp4ServiceBindingProto,
                                      (void**)&Dhcp4ServiceBinding);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error getting Dhcp4ServiceBinding protocol: %s",
             error_str[error_efi_to_generic(Status)]);
         return Status;
      }
   }

   if (Dhcp4Handle == NULL) {
      Status = Dhcp4ServiceBinding->CreateChild(Dhcp4ServiceBinding,
                                                &Dhcp4Handle);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error creating Dhcp4 child: %s",
             error_str[error_efi_to_generic(Status)]);
         return Status;
      }
   }

   if (Dhcp4 == NULL) {
      Status = get_protocol_interface(Dhcp4Handle, &Dhcp4Proto, (void**)&Dhcp4);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error getting Dhcp4 protocol: %s",
             error_str[error_efi_to_generic(Status)]);
         return Status;
      }
   }

   /*
    * Kick off or resume the DHCP configuration process and babysit until
    * an address is bound.
    */
   for (;;) {
      Status = Dhcp4->GetModeData(Dhcp4, &Dhcp4ModeData);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error in Dhcp4->GetModeData: %s",
             error_str[error_efi_to_generic(Status)]);
         return Status;
      }

      switch (Dhcp4ModeData.State) {
      case Dhcp4Stopped:
         Log(LOG_DEBUG, "Doing Dhcp4->Configure");
         memset(&Dhcp4CfgData, 0, sizeof(Dhcp4CfgData));
         Status = Dhcp4->Configure(Dhcp4, &Dhcp4CfgData);
         if (EFI_ERROR(Status)) {
            Log(LOG_ERR, "Error in Dhcp4->Configure: %s",
                error_str[error_efi_to_generic(Status)]);
            return Status;
         }
         break;

      case Dhcp4Init:
      case Dhcp4InitReboot:
         Log(LOG_DEBUG, "Doing Dhcp4->Start");
         Status = Dhcp4->Start(Dhcp4, NULL);
         if (EFI_ERROR(Status)) {
            Log(LOG_ERR, "Error in Dhcp4->Start: %s",
                error_str[error_efi_to_generic(Status)]);
            return Status;
         }
         break;

      case Dhcp4Selecting:
      case Dhcp4Requesting:
      case Dhcp4Rebinding:
      case Dhcp4Rebooting:
      case Dhcp4Renewing:
         bs->Stall(100000);
         break;

      case Dhcp4Bound:
         Log(LOG_DEBUG, "%02x:%02x:%02x:%02x:%02x:%02x -> cli=%u.%u.%u.%u "
             "svr=%u.%u.%u.%u rtr=%u.%u.%u.%u sbn=%u.%u.%u.%u lse=%u",
             Dhcp4ModeData.ClientMacAddress.Addr[0],
             Dhcp4ModeData.ClientMacAddress.Addr[1],
             Dhcp4ModeData.ClientMacAddress.Addr[2],
             Dhcp4ModeData.ClientMacAddress.Addr[3],
             Dhcp4ModeData.ClientMacAddress.Addr[4],
             Dhcp4ModeData.ClientMacAddress.Addr[5],
             Dhcp4ModeData.ClientAddress.Addr[0],
             Dhcp4ModeData.ClientAddress.Addr[1],
             Dhcp4ModeData.ClientAddress.Addr[2],
             Dhcp4ModeData.ClientAddress.Addr[3],
             Dhcp4ModeData.ServerAddress.Addr[0],
             Dhcp4ModeData.ServerAddress.Addr[1],
             Dhcp4ModeData.ServerAddress.Addr[2],
             Dhcp4ModeData.ServerAddress.Addr[3],
             Dhcp4ModeData.RouterAddress.Addr[0],
             Dhcp4ModeData.RouterAddress.Addr[1],
             Dhcp4ModeData.RouterAddress.Addr[2],
             Dhcp4ModeData.RouterAddress.Addr[3],
             Dhcp4ModeData.SubnetMask.Addr[0],
             Dhcp4ModeData.SubnetMask.Addr[1],
             Dhcp4ModeData.SubnetMask.Addr[2],
             Dhcp4ModeData.SubnetMask.Addr[3],
             Dhcp4ModeData.LeaseTime);
         Dhcp4NicHandle = NicHandle;
         return EFI_SUCCESS;
      }
   }
}

/*-- get_ipv4_addr -------------------------------------------------------------
 *
 *      If the given NIC doesn't have an IPv4 address (yet), kick off DHCPv4
 *      and wait for it to complete.
 *
 * Parameters
 *      IN  NicHandle: Handle for NIC to use.
 *
 * Results
 *      EFI_SUCCESS or an error.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_ipv4_addr(EFI_HANDLE NicHandle)
{
   EFI_STATUS Status;

   Status = has_ipv4_addr(NicHandle);
   if (Status != EFI_NO_MAPPING) {
      return Status;
   }
   Status = set_policy_dhcp4(NicHandle);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   return run_dhcpv4(NicHandle);
}
