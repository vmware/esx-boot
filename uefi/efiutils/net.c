/*******************************************************************************
 * Copyright (c) 2008-2013,2015,2019-2020,2022 VMware, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * net.c -- Network-related UEFI functions
 */

#include <stdio.h>
#include "efi_private.h"
#include "Protocol/SimpleNetwork.h"
#include "Protocol/PciIo.h"
#include "protocol/gpxe_download.h"

#define BOOTIF_OPTION_SIZE  28  /* STRSIZE("BOOTIF=xx-aa-bb-cc-dd-ee-ff") */

static EFI_GUID SimpleNetworkProto = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
static EFI_GUID PciIoProto = EFI_PCI_IO_PROTOCOL_GUID;

static char ipappend[BOOTIF_OPTION_SIZE];

/*-- is_network_boot -----------------------------------------------------------
 *
 *      Check whether we are booted from the network:
 *        1. Check if we have been PXE or HTTP booted
 *        2. Check if we have been gPXE booted
 *
 * Results
 *      True/false
 *----------------------------------------------------------------------------*/
bool is_network_boot(void)
{
   EFI_HANDLE BootVolume;
   EFI_STATUS Status;
   VOID *Proto;

   if (is_http_boot()) {
      return true;
   }

   Status = get_boot_volume(&BootVolume);
   if (EFI_ERROR(Status)) {
      return false;
   }

   Status = get_protocol_interface(BootVolume, &SimpleNetworkProto, &Proto);
   if (!EFI_ERROR(Status)) {
      return true;
   }

   Status = get_protocol_interface(BootVolume, &GpxeDownloadProto, &Proto);

   return !EFI_ERROR(Status);
}

/*-- get_bootif_option ---------------------------------------------------------
 *
 *      Returns the same string as the pxelinux "ipappend 2" option would have
 *      added to the kernel command line.
 *
 *      The result is a string of the form "BOOTIF=xx-aa-bb-cc-dd-ee-ff", where
 *      xx is the Hardware Type Number of the boot interface (see RFC 1700), and
 *      aa:bb:cc:dd:ee:ff is its MAC address.
 *
 * Parameters
 *      OUT bootif: a pointer to the statical BOOTIF string.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_bootif_option(const char **bootif)
{
   EFI_SIMPLE_NETWORK *Network;
   EFI_HANDLE BootVolume, Nic;
   int ipv;
   EFI_MAC_ADDRESS mac;
   UINT8 macType = MAC_UNKNOWN;
   EFI_STATUS Status;

   Status = get_boot_volume(&BootVolume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (is_http_boot()) {
      Status = get_http_nic_info(BootVolume, &Nic, &ipv, &mac, &macType);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }
   } else {
      Nic = BootVolume;
   }

   if (macType == MAC_UNKNOWN) {
      Status = get_protocol_interface(Nic, &SimpleNetworkProto,
                                      (VOID **)&Network);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }
      macType = Network->Mode->IfType;
      mac = Network->Mode->CurrentAddress;
   }

   snprintf(ipappend, sizeof (ipappend),
            "BOOTIF=%02x-%02x-%02x-%02x-%02x-%02x-%02x",
            macType, mac.Addr[0], mac.Addr[1], mac.Addr[2],
            mac.Addr[3], mac.Addr[4], mac.Addr[5]);

   *bootif = ipappend;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- disable_network_controllers --------------------------------------------
 *
 *      Find all PCI network controllers in the platform, disconnect
 *      their drivers, and disable bus-mastering for each.  Ignore
 *      errors.  Warning: may break any active iSCSI or FCoE
 *      connections and result in failure to complete an iSCSI or FCoE
 *      boot/install, so call only if needed to work around other bugs
 *      (PR 1424506).
 *----------------------------------------------------------------------------*/
void disable_network_controllers(void)
{
   EFI_STATUS Status;
   UINTN HandleCount;
   EFI_HANDLE *HandleBuffer;
   UINTN h;

   Log(LOG_DEBUG, "Disabling network controller DMA");

   /*
    * Find all network handles.
    */
   Status = LocateHandleByProtocol(&SimpleNetworkProto,
                                   &HandleCount, &HandleBuffer);
   if (EFI_ERROR(Status)) {
      return;
   }

   for (h = 0; h < HandleCount; h++) {
      EFI_HANDLE nicHandle = HandleBuffer[h];
      EFI_DEVICE_PATH *nicDevicePath;
      EFI_HANDLE nicPciDevice;
      EFI_PCI_IO_PROTOCOL *nicPciIo;

      /*
       * Get the device path for this handle.
       */
      Status = devpath_get(nicHandle, &nicDevicePath);
      if (EFI_ERROR(Status)) {
         continue;
      }

      /*
       * Find PCI device on this device path.
       */
      Status = bs->LocateDevicePath(&PciIoProto, &nicDevicePath,
                                    &nicPciDevice);
      if (EFI_ERROR(Status)) {
         continue;
      }

      /*
       * Check that the PCI device found is a NIC and that it's the
       * last device on the path.
       */
      if (nicDevicePath->Type != MESSAGING_DEVICE_PATH ||
          nicDevicePath->SubType != MSG_MAC_ADDR_DP ||
          !IsDevPathEnd(NextDevPathNode(nicDevicePath))) {
         continue;
      }

      /*
       * Disconnect drivers from the NIC.
       */
      Status = bs->DisconnectController(nicHandle, NULL, NULL);
      Log(LOG_DEBUG, "Disconnect drivers from %p: %zx", nicHandle, Status);
      if (EFI_ERROR(Status)) {
         continue;
      }

      /*
       * Disable bus mastering for the NIC.
       */
      Status = get_protocol_interface(nicPciDevice, &PciIoProto,
                                      (void**)&nicPciIo);
      if (EFI_ERROR(Status)) {
         continue;
      }

      Status = nicPciIo->Attributes(nicPciIo,
                                    EfiPciIoAttributeOperationDisable,
                                    EFI_PCI_IO_ATTRIBUTE_BUS_MASTER, NULL);
      Log(LOG_DEBUG, "Disable bus mastering on %p: %zx", nicPciDevice, Status);
      if (EFI_ERROR(Status)) {
         continue;
      }
   }

   bs->FreePool(HandleBuffer);
}
