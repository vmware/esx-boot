/*******************************************************************************
 * Copyright (c) 2019-2020,2022-2023 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * dhcpv4.c -- Support for making a DHCPv4 request.
 */

#include "efi_private.h"
#include "ServiceBinding.h"
#include "Dhcp.h"
#include "Dhcp4.h"
#include "Ip4Config2.h"

static EFI_GUID Dhcp4ServiceBindingProto =
   EFI_DHCP4_SERVICE_BINDING_PROTOCOL_GUID;
static EFI_GUID Dhcp4Proto = EFI_DHCP4_PROTOCOL_GUID;
static EFI_GUID Ip4Config2Proto = EFI_IP4_CONFIG2_PROTOCOL_GUID;

static const EFI_IPv4_ADDRESS Zero = { .Addr = {0, 0, 0, 0} };
#define IsZero(addr) (memcmp(&addr, &Zero, sizeof(Zero)) == 0)

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
   EFI_IP4_CONFIG2_INTERFACE_INFO *Info;
   EFI_IP4_CONFIG2_POLICY Policy;
   static bool infoLogged = false, policyLogged = false;

   Status = get_protocol_interface(NicHandle, &Ip4Config2Proto,
                                   (void**)&Ip4Config2);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting Ip4Config2 protocol: %s",
          error_str[error_efi_to_generic(Status)]);
      return Status;
   }

   if (!policyLogged) {
      DataSize = sizeof(Policy);
      Status = Ip4Config2->GetData(Ip4Config2, Ip4Config2DataTypePolicy,
                                   &DataSize, &Policy);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error in Ip4Config2->GetData (policy): %s",
             error_str[error_efi_to_generic(Status)]);
      } else {
         Log(LOG_DEBUG, "ip4config2 policy=%u", Policy);
      }
      policyLogged = true;
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

   if (!IsZero(Info->StationAddress)) {
      if (!infoLogged) {
         EFI_IP4_ROUTE_TABLE *rtab = Info->RouteTable;
         UINT32 rtabsize = rtab == NULL ? 0 : Info->RouteTableSize;
         Log(LOG_DEBUG, "ip4config2 ift=%u msz=%u "
             "mac=%02x:%02x:%02x:%02x:%02x:%02x "
             "adr=%u.%u.%u.%u sbn=%u.%u.%u.%u rts=%u",
             Info->IfType, Info->HwAddressSize,
             Info->HwAddress.Addr[0], Info->HwAddress.Addr[1],
             Info->HwAddress.Addr[2], Info->HwAddress.Addr[3],
             Info->HwAddress.Addr[4],Info->HwAddress.Addr[5],
             Info->StationAddress.Addr[0], Info->StationAddress.Addr[1],
             Info->StationAddress.Addr[2], Info->StationAddress.Addr[3],
             Info->SubnetMask.Addr[0], Info->SubnetMask.Addr[1],
             Info->SubnetMask.Addr[2], Info->SubnetMask.Addr[3],
             Info->RouteTableSize);

         for (UINT32 i = 0; i < rtabsize; i++) {
            Log(LOG_DEBUG, "  route: adr=%u.%u.%u.%u sbn=%u.%u.%u.%u "
                "gwa=%u.%u.%u.%u",
                rtab[i].SubnetAddress.Addr[0], rtab[i].SubnetAddress.Addr[1],
                rtab[i].SubnetAddress.Addr[2], rtab[i].SubnetAddress.Addr[3],
                rtab[i].SubnetMask.Addr[0], rtab[i].SubnetMask.Addr[1],
                rtab[i].SubnetMask.Addr[2], rtab[i].SubnetMask.Addr[3],
                rtab[i].GatewayAddress.Addr[0], rtab[i].GatewayAddress.Addr[1],
                rtab[i].GatewayAddress.Addr[2], rtab[i].GatewayAddress.Addr[3]);
         }
         infoLogged = true;
      }
   } else {
      Log(LOG_DEBUG, "No local IPv4 address");
      Status = EFI_NO_MAPPING;
   }

   sys_free(Info);
   return Status;
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
 *      IN  NicHandle:     Handle for NIC to use.
 *      IN  preferredAddr: Preferred IPv4 address; 0.0.0.0 if no preference.
 *
 * Results
 *      EFI_SUCCESS or an error.
 *----------------------------------------------------------------------------*/
EFI_STATUS run_dhcpv4(EFI_HANDLE NicHandle, EFI_IPv4_ADDRESS preferredAddr)
{
   EFI_STATUS Status;
   EFI_DHCP4_CONFIG_DATA Dhcp4CfgData;
   EFI_DHCP4_MODE_DATA Dhcp4ModeData;
   EFI_DHCP4_STATE prevState = (EFI_DHCP4_STATE) -1;

   /*
    * DHCP options; see RFC 2132.  EFI_DHCP4_PACKET_OPTION contains a logically
    * variable length Data array, so it's awkward to initialize if declared as
    * the proper type.  Work around that by declaring it as UINT8[].
    */
   static /* EFI_DHCP4_PACKET_OPTION */ UINT8 prq[] = {
      DHCP4_TAG_PARA_LIST, // OpCode
      3,                   // Length
      DHCP4_TAG_NETMASK,   // Data
      DHCP4_TAG_ROUTER,
      DHCP4_TAG_DNS_SERVER
   };
   static EFI_DHCP4_PACKET_OPTION *options[] = {
      (EFI_DHCP4_PACKET_OPTION *)prq
   };

   if (Dhcp4NicHandle != NULL && NicHandle != Dhcp4NicHandle) {
      Log(LOG_DEBUG, "New NicHandle; calling dhcp4_cleanup");
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
      if (Dhcp4ModeData.State != prevState) {
         Log(LOG_DEBUG, "Dhcp4ModeData.State %u", Dhcp4ModeData.State);
      }

      switch (Dhcp4ModeData.State) {
      case Dhcp4Stopped:
         Log(LOG_DEBUG, "Doing Dhcp4->Configure");
         memset(&Dhcp4CfgData, 0, sizeof(Dhcp4CfgData));
         Dhcp4CfgData.OptionList = options;
         Dhcp4CfgData.OptionCount = ARRAYSIZE(options);
         Dhcp4CfgData.ClientAddress = preferredAddr;
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

#if DEBUG || DHCPv4_DEBUG
         if (Dhcp4ModeData.ReplyPacket) {
            EFI_DHCP4_PACKET *p = Dhcp4ModeData.ReplyPacket;
            EFI_DHCP4_HEADER *h = &p->Dhcp4.Header;
            Log(LOG_DEBUG, "DHCP reply siz=%u len=%u opc=%u hwt=%u hwl=%u "
                "hps=%u xid=%u sec=%u res=%u cla=%u.%u.%u.%u yra=%u.%u.%u.%u "
                "sva=%u.%u.%u.%u raa=%u.%u.%u.%u "
                "mac=%02x:%02x:%02x:%02x:%02x:%02x "
                "svn=\"%s\" bfn=\"%s\" mgc=%08x",
                p->Size, p->Length, h->OpCode, h->HwType, h->HwAddrLen,
                h->Hops, h->Xid, h->Seconds, h->Reserved,
                h->ClientAddr.Addr[0], h->ClientAddr.Addr[1],
                h->ClientAddr.Addr[2], h->ClientAddr.Addr[3],
                h->YourAddr.Addr[0], h->YourAddr.Addr[1],
                h->YourAddr.Addr[2], h->YourAddr.Addr[3],
                h->ServerAddr.Addr[0], h->ServerAddr.Addr[1],
                h->ServerAddr.Addr[2], h->ServerAddr.Addr[3],
                h->GatewayAddr.Addr[0], h->GatewayAddr.Addr[1],
                h->GatewayAddr.Addr[2], h->GatewayAddr.Addr[3],
                h->ClientHwAddr[0], h->ClientHwAddr[1],
                h->ClientHwAddr[2], h->ClientHwAddr[3],
                h->ClientHwAddr[4], h->ClientHwAddr[5],
                h->ServerName, h->BootFileName, p->Dhcp4.Magik);
            log_data(LOG_DEBUG, p->Dhcp4.Option,
                     p->Length - sizeof(EFI_DHCP4_HEADER) - sizeof(UINT32));
         }
#endif
         Dhcp4NicHandle = NicHandle;

         Status = has_ipv4_addr(NicHandle);
         if (Status != EFI_SUCCESS) {
            /*
             * XXX This is actually happening, but moments later the IP address
             * is used by HTTP and working!  It seems some magic in the network
             * stack propagates the address in the background or pulls it out
             * of the DHCP object on demand.
             */
            Log(LOG_DEBUG, "Dhcp4Bound but IP address not set (yet): %s",
                error_str[error_efi_to_generic(Status)]);
         }

         return EFI_SUCCESS;
      }
      prevState = Dhcp4ModeData.State;
   }
}

/*-- get_ipv4_addr -------------------------------------------------------------
 *
 *      If the given NIC doesn't have an IPv4 address (yet), kick off DHCPv4
 *      and wait for it to complete.
 *
 * Parameters
 *      IN  NicHandle: Handle for NIC to use.
 *      IN  preferredAddr: Preferred IPv4 address; 0.0.0.0 if no preference.
 *
 * Results
 *      EFI_SUCCESS or an error.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_ipv4_addr(EFI_HANDLE NicHandle, EFI_IPv4_ADDRESS preferredAddr)
{
   EFI_STATUS Status;

   Status = has_ipv4_addr(NicHandle);
   if (Status != EFI_NO_MAPPING) {
      return Status;
   }
   return run_dhcpv4(NicHandle, preferredAddr);
}
