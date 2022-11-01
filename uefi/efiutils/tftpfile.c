/*******************************************************************************
 * Copyright (c) 2008-2011,2013-2015,2021-2022 VMware, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tftpfile.c --
 *
 *      Provides access to files through the (M)TFTP and PXE Base Code (PXE BC)
 *      Protocols.
 *
 *      Ideally, all files could be loaded using the Load File Protocol,
 *      regardless of the underlying media/transport type. However, the EFI
 *      spec explicitly states that the PXE BC's implementation of LoadFile()
 *      may only be used to discover and retrieve the bootstrap program
 *      (i.e. this bootloader). Other files (e.g. the kernel) must be retrieved
 *      using explicit TFTP calls. That's the purpose of this file.
 *
 *      The following code does not perform a complete PXE exchange. Instead,
 *      the assumption is made that if this is a PXE boot, the PXE BC and MTFTP
 *      modules must have been initialized and the PXE protocol carried out in
 *      order to discover and download this program. Therefore, after proper
 *      sanity checks, the TFTP functionality is accessed directly.
 */

#include <string.h>
#include <arpa/inet.h>
#include "efi_private.h"

/*
 * Standard DHCPv4 options
 *
 * Option 0: Pad Option
 *
 * "The pad option can be used to cause subsequent fields to align on word
 * boundaries.  The code for the pad option is 0, and its length is 1 octet."
 */
#define OPT_PAD 0

/*
 * Option 255: End Option
 *
 * "The end option marks the end of valid information in the vendor field.
 * Subsequent octets should be filled with pad options.  The code for the end
 * option is 255, and its length is 1 octet."
 */
#define OPT_END 255

/* Option 54: Server Identifier
 *
 * "DHCP clients use the contents of the 'server identifier' field as the
 * destination address for any DHCP messages unicast to the DHCP
 * server. ... The identifier is the IP address of the selected server."
 */
#define OPT_SERVER_IDENTIFIER 54

/*
 * Standard DHCPv6 options
 *
 * Option 59: boot-file-url
 *   This string is the URL for the boot file. It MUST comply with STD 66
 *   [RFC3986]. The string is not NUL-terminated.
 */
#define OPT_BOOTFILE_URL 59

/*
 * RFC3986 specifies that URLs should not be longer than 255 characters.
 * Let's tolerate more, in case a DHCP server chose to ignore that.
 */
#define URL_SIZE_MAX 1024

/*
 * TFTP block size to request (RFC 2348).  The server can always choose to use
 * a smaller size.  Using a large block size makes transfers faster by
 * increasing the amount of data that is transferred per ack.  It also allows
 * larger files to be transferred without sequence number wrapping, which not
 * all servers support.  On the other hand, if the block size is larger than
 * the path MTU, the blocks will be fragmented at the IP level, so if the
 * network is lossy, that increases the probability of the block needing to be
 * retransmitted because a fragment was lost.
 *
 * The default value set here can be overridden by calling tftp_set_block_size.
 *
 * Contrary to the EFI spec, the EDK implementation of Mtftp() will not
 * negotiate the largest block size with the server if the BlockSize argument
 * is NULL.  Further, the elilo sources mention that some real firmware
 * implementations timeout when given a NULL BlockSize.  Therefore, we always
 * pass in an explicit BlockSize request, never NULL.
 */
#define TFTP_BLKSIZE_MIN 512   // defined by UEFI standard
#define TFTP_BLKSIZE_MAX 65464 // defined by RFC 2348
static UINTN tftp_block_size = 1468; // default; fits in 1500 byte MTU

static bool isIPv6 = false;

/*-- tftp_set_block_size  ------------------------------------------------------
 *
 *      Set the blksize option value to be used in TFTP requests.
 *
 * Parameters
 *      IN size:  block size
 *----------------------------------------------------------------------------*/
void tftp_set_block_size(size_t blksize)
{
   if (blksize < TFTP_BLKSIZE_MIN || blksize > TFTP_BLKSIZE_MAX) {
      Log(LOG_WARNING,
          "Requested TFTP blksize %zu not in range %u-%u; using %zu",
          blksize, TFTP_BLKSIZE_MIN, TFTP_BLKSIZE_MAX, tftp_block_size);
      return;
   }
   Log(LOG_DEBUG, "Switching TFTP blksize from %zu to %zu",
       tftp_block_size, blksize);
   tftp_block_size = blksize;
}


/*-- get_ipv6_boot_url ---------------------------------------------------------
 *
 *      Retrieve the IPv6 boot file URL from a PXE BC packet. The URL format is
 *      specified in RFC3986. It looks like:
 *
 *      tftp:://[ipv6]/path/to/bootfile
 *
 * Parameters
 *      IN Packet:  pointer to a PXE BC packet
 *      OUT urlBuf: buffer where to copy the boot file URL
 *
 * Results
 *      A pointer to the boot file URL within the output buffer.
 *----------------------------------------------------------------------------*/
static const char *get_ipv6_boot_url(const EFI_PXE_BASE_CODE_PACKET *Packet,
                                     char urlBuf[URL_SIZE_MAX])
{
   const uint8_t *p;
   const uint8_t *optEnd;
   uint16_t optCode, optLen;

   p = (uint8_t *)Packet->Dhcpv6.DhcpOptions;
   optEnd = p + sizeof (Packet->Dhcpv6.DhcpOptions);

   while (p < optEnd) {
      optCode = ntohs(*(uint16_t *)p);
      p += sizeof(optCode);

      optLen = ntohs(*(uint16_t *)p);
      p += sizeof(optLen);

      // Protect from bogus/malicious length in DHCP option packets
      optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);

      if (optCode == OPT_BOOTFILE_URL) {
         // Per RFC3986, option string is not NULL-terminated.
         optLen = MIN(optLen, URL_SIZE_MAX - 1);
         memcpy(urlBuf, p, optLen);
         urlBuf[optLen] = '\0';
         return urlBuf;
      }

      p += optLen;
   }

   return NULL;
}

/*-- get_tftp_ipv6_addr --------------------------------------------------------
 *
 *      Retrieve the tftp server IPv6 address from a PXE BC packet.
 *
 * Parameters
 *      IN Packet:    pointer to a PXE BC packet
 *      OUT ServerIp: pointer to a buffer where to copy the IPv6 address
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_tftp_ipv6_addr(const EFI_PXE_BASE_CODE_PACKET *Packet,
                                     EFI_IP_ADDRESS *ServerIp)
{
   char url[URL_SIZE_MAX];
   const char *ip;
   int status;

   if (get_ipv6_boot_url(Packet, url) == NULL) {
      return EFI_NOT_FOUND;
   }

   // ipv6 is enclosed in '[]' (by RFC 3986)
   ip = strchr(url, '[');
   if (ip == NULL) {
      return EFI_NOT_FOUND;
   }
   ip++;

   status = inet_pton(AF_INET6, ip, &ServerIp->v6);
   return (status == 1) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/*-- get_pxe_base_code_packet --------------------------------------------------
 *
 *      Return a PXE Base Code Packet (if any), associated with a given handle.
 *      This packet contains the network specific information related to the
 *      PXE BC protocol.
 *
 * Parameters
 *      IN Pxe:     pointer to the PXE BC interface
 *      OUT Packet: pointer to the PXE BC packet.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_pxe_base_code_packet(EFI_PXE_BASE_CODE *Pxe,
                                           EFI_PXE_BASE_CODE_PACKET **Packet)
{
   EFI_PXE_BASE_CODE_MODE *PxeMode;

   EFI_ASSERT_PARAM(Pxe != NULL);
   EFI_ASSERT_PARAM(Packet != NULL);

   *Packet = NULL;
   PxeMode = Pxe->Mode;
   if (!PxeMode->Started) {
      return EFI_NOT_STARTED;
   }

   if (PxeMode->UsingIpv6) {
      isIPv6 = true;
   }

   if (PxeMode->PxeReplyReceived) {
      *Packet = &PxeMode->PxeReply;
   } else if (PxeMode->ProxyOfferReceived) {
      *Packet = &PxeMode->ProxyOffer;
   } else if (PxeMode->DhcpAckReceived) {
      *Packet = &PxeMode->DhcpAck;
   } else {
      return EFI_UNSUPPORTED;
   }

   return EFI_SUCCESS;
}

/*-- get_ipv4_dhcp_ip ----------------------------------------------------------
 *
 *      Retrieve the IPv4 DHCP server IP from the cached options in
 *      the PXE BC packet.
 *
 * Parameters
 *      IN Packet:    pointer to a PXE BC packet
 *      OUT ServerIP: the IPv4 address of the DHCP server; 0.0.0.0 on error.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_ipv4_dhcp_ip(const EFI_PXE_BASE_CODE_PACKET *Packet,
                                   EFI_IP_ADDRESS *ServerIp)
{
   const uint8_t *p;
   const uint8_t *optEnd;
   uint8_t optCode, optLen;

   p = (uint8_t *)Packet->Dhcpv4.DhcpOptions;
   optEnd = p + sizeof (Packet->Dhcpv4.DhcpOptions);

   while (p < optEnd) {
      optCode = *p++;

      if (optCode == OPT_PAD) {
         continue;
      }
      if (optCode == OPT_END) {
         break;
      }

      optLen = *p++;

      // Protect from bogus/malicious length in DHCP option packets
      optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);

      if (optCode == OPT_SERVER_IDENTIFIER &&
          optLen == sizeof(ServerIp->v4)) {

         memcpy(ServerIp, p, optLen);
         return EFI_SUCCESS;
      }

      p += optLen;
   }

   memset(ServerIp, 0, sizeof(ServerIp));
   return EFI_NOT_FOUND;
}



/*-- get_pxe_info --------------------------------------------------------------
 *
 *      Gather info about the PXE BC instance (if any) attached to the given
 *      handle.
 *
 * Parameters
 *      OUT Pxe:      pointer to the PXE BC interface
 *      OUT ServerIp: the IPv4 address of the PXE server
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_pxe_info(EFI_PXE_BASE_CODE **Pxe,
                               EFI_IP_ADDRESS *ServerIp)
{
   EFI_PXE_BASE_CODE_PACKET *Packet;
   EFI_STATUS Status;
   static EFI_IP_ADDRESS lastServerIp;

   EFI_ASSERT_PARAM(Pxe != NULL);
   EFI_ASSERT_PARAM(ServerIp != NULL);

   if (!is_pxe_boot(Pxe)) {
      return EFI_NOT_FOUND;
   }

   Status = get_pxe_base_code_packet(*Pxe, &Packet);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   memset(ServerIp, 0, sizeof(ServerIp));
   if (isIPv6) {
      Status = get_tftp_ipv6_addr(Packet, ServerIp);
      if (EFI_ERROR(Status)) {
         return Status;
      }

      if (memcmp(&lastServerIp, ServerIp, sizeof(ServerIp)) != 0) {
         Log(LOG_DEBUG, "tftp6 server IP %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
             ServerIp->v6.Addr[0], ServerIp->v6.Addr[1],
             ServerIp->v6.Addr[2], ServerIp->v6.Addr[3],
             ServerIp->v6.Addr[4], ServerIp->v6.Addr[5],
             ServerIp->v6.Addr[6], ServerIp->v6.Addr[7],
             ServerIp->v6.Addr[8], ServerIp->v6.Addr[9],
             ServerIp->v6.Addr[10], ServerIp->v6.Addr[11],
             ServerIp->v6.Addr[12], ServerIp->v6.Addr[13],
             ServerIp->v6.Addr[14], ServerIp->v6.Addr[15]);
      }

   } else {
      /*
       * Look first at the SiAddr field of the DHCPv4 packet (next-server).  In
       * the unlikely case this field is not filled in, fall back to the DHCP
       * server's own address.  See UEFI 2.5 section E.4.20.2.
       */
      memcpy(&ServerIp->v4, Packet->Dhcpv4.BootpSiAddr,
             sizeof(ServerIp->v4));

      if (ServerIp->v4.Addr[0] == 0 &&
          ServerIp->v4.Addr[1] == 0 &&
          ServerIp->v4.Addr[2] == 0 &&
          ServerIp->v4.Addr[3] == 0) {
         Status = get_ipv4_dhcp_ip(Packet, ServerIp);
         if (EFI_ERROR(Status)) {
            return Status;
         }
      }

      if (memcmp(&lastServerIp, ServerIp, sizeof(ServerIp)) != 0) {
         Log(LOG_DEBUG, "tftp4 server IP %u.%u.%u.%u",
             ServerIp->v4.Addr[0], ServerIp->v4.Addr[1],
             ServerIp->v4.Addr[2], ServerIp->v4.Addr[3]);
      }
   }
   memcpy(&lastServerIp, ServerIp, sizeof(ServerIp));

   return EFI_SUCCESS;
}

/*-- is_pxe_boot ---------------------------------------------------------------
 *
 *      Check whether we are PXE booting.
 *
 * Parameters
 *      OUT Pxe: pointer to the PXE BC interface
 *
 * Results
 *      True if PXE booting, else false.
 *----------------------------------------------------------------------------*/
bool is_pxe_boot(EFI_PXE_BASE_CODE **Pxe)
{
   EFI_GUID PxeBaseCodeProto = EFI_PXE_BASE_CODE_PROTOCOL_GUID;
   EFI_PXE_BASE_CODE *PxeProtocol;
   EFI_HANDLE BootVolume;
   EFI_STATUS Status;

   Status = get_boot_volume(&BootVolume);
   if (EFI_ERROR(Status)) {
      return false;
   }

   Status = get_protocol_interface(BootVolume, &PxeBaseCodeProto,
                                   (void **)&PxeProtocol);

   if (Pxe != NULL) {
      *Pxe = EFI_ERROR(Status) ? NULL : PxeProtocol;
   }

   return !EFI_ERROR(Status);
}

/*-- get_pxe_boot_file ---------------------------------------------------------
 *
 *      Return the boot file name. This is the boot file name that has been
 *      sent by the DHCP server to the client during initial PXE setup.
 *
 *      NOTE: On any error, this function returns an empty boot file name.
 *
 * Parameters
 *      IN Pxe:       pointer to the PXE BC interface
 *      OUT Bootfile: pointer to the buffer containing the boot file name.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_pxe_boot_file(EFI_PXE_BASE_CODE *Pxe, CHAR16 **BootFile)
{
   EFI_PXE_BASE_CODE_PACKET *Packet;
   EFI_STATUS Status;

   EFI_ASSERT_PARAM(Pxe != NULL);
   EFI_ASSERT_PARAM(BootFile != NULL);

   Status = get_pxe_base_code_packet(Pxe, &Packet);
   if (EFI_ERROR(Status)) {
      return ucs2_alloc(0, BootFile);
   }

   if (isIPv6) {
      char url[URL_SIZE_MAX];
      const char *ip;

      if (get_ipv6_boot_url(Packet, url) == NULL) {
         return EFI_NOT_FOUND;
      }

      // bootfile path starts after']' (by RFC 3986)
      ip = strchr(url, ']');
      if (ip == NULL) {
         return EFI_NOT_FOUND;
      }
      ip++;
      *BootFile = NULL;
      return ascii_to_ucs2(ip, BootFile);
   } else {
      *BootFile = NULL;
      return ascii_to_ucs2(((char *)Packet->Dhcpv4.BootpBootFile), BootFile);
   }
}

/*-- tftp_file_get_size --------------------------------------------------------
 *
 *      Get the size of a file using TFTP.
 *
 *      If downloading from a TFTP server without the 'tsize' option, it is
 *      possible that this could end up downloading the whole file and throwing
 *      its contents away. Get yourself a better TFTP server!
 *
 * Parameters
 *      IN  Volume:   handle to the volume from which to load the file
 *      IN  filepath: the ASCII absolute path of the file to retrieve
 *      OUT FileSize: file size in bytes
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS tftp_file_get_size(UNUSED_PARAM(EFI_HANDLE Volume),
                              const char *filepath, UINTN *FileSize)
{
   EFI_PXE_BASE_CODE *Pxe;
   EFI_IP_ADDRESS ServerIp;
   /* Some firmware doesn't like a NULL BufferPtr in the call to Mtftp(). */
   UINT8 DummyBuf;
   UINT64 Size = 0;
   EFI_STATUS Status;

   EFI_ASSERT_PARAM(filepath != NULL);
   EFI_ASSERT_PARAM(FileSize != NULL);

   Status = get_pxe_info(&Pxe, &ServerIp);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   efi_set_watchdog_timer(WATCHDOG_DISABLE);

   Status = Pxe->Mtftp(Pxe, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
                       &DummyBuf, FALSE, &Size, &tftp_block_size,
                       &ServerIp, (UINT8 *)filepath, NULL, TRUE);

   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   /*
    * Some firmware returns EFI_BUFFER_TOO_SMALL even though it successfully
    * managed to get the size.
    */
   if (Status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(Status)) {
      return Status;
   }

   *FileSize = (UINTN)Size;

   return EFI_SUCCESS;
}

/*-- tftp_file_load ------------------------------------------------------------
 *
 *      Load a file into memory using TFTP. UEFI watchdog timer is disabled
 *      during the Mtftp() operation, so it does not trigger and reboot the
 *      platform during large/slow file transfers.
 *
 * Parameters
 *      IN  Volume:   handle to the volume from which to load the file
 *      IN  filepath: the ASCII absolute path of the file to retrieve
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      OUT Buffer:   pointer to where to load the file
 *      IN  BufSize:  output buffer size in bytes
 *      OUT BufSize:  number of bytes that have been written into Buffer
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS tftp_file_load(EFI_HANDLE Volume, const char *filepath,
                          int (*callback)(size_t), VOID **Buffer,
                          UINTN *BufSize)
{
   EFI_PXE_BASE_CODE *Pxe;
   EFI_IP_ADDRESS ServerIp;
   VOID *Data;
   UINTN Size;
   UINT64 Size64;
   EFI_STATUS Status;
   int error;

   EFI_ASSERT_PARAM(filepath != NULL);
   EFI_ASSERT_PARAM(Buffer != NULL);
   EFI_ASSERT_PARAM(BufSize != NULL);

   Status = tftp_file_get_size(Volume, filepath, &Size);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = get_pxe_info(&Pxe, &ServerIp);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Data = sys_malloc(Size);
   if (Data == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   Size64 = Size;
   efi_set_watchdog_timer(WATCHDOG_DISABLE);

   Status = Pxe->Mtftp(Pxe, EFI_PXE_BASE_CODE_TFTP_READ_FILE,
                       Data, FALSE, &Size64, &tftp_block_size,
                       &ServerIp, (UINT8 *)filepath, NULL, FALSE);

  efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   if (EFI_ERROR(Status)) {
      sys_free(Data);
      return Status;
   }

   /*
    * XXX: Use EFI_PXE_BASE_CODE_CALLBACK to call the progress callback each
    * time a packet is received.
    */
   if (callback != NULL) {
      error = callback((size_t)Size64);
      if (error != 0) {
         sys_free(Data);
         return error_generic_to_efi(error);
      }
   }

   *Buffer = Data;
   *BufSize = (UINTN)Size64;

   return EFI_SUCCESS;
}
