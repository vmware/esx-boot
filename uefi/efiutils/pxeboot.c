/*******************************************************************************
 * Copyright (c) 2008-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * pxeboot.c --
 *
 *      Provides access to data available in the PXE Base Code (PXE BC)
 *      Protocols.
 *
 *      Ideally, all files could be loaded using the Load File Protocol,
 *      regardless of the underlying media/transport type. However, the EFI
 *      spec explicitly states that the PXE BC's implementation of LoadFile()
 *      may only be used to discover and retrieve the bootstrap program
 *      (i.e. this bootloader). Other files (e.g. the kernel) must be retrieved
 *      using explicit TFTP calls.
 *
 *      The assumption in the following code is that if this is a PXE boot, the
 *      PXE BC and MTFTP modules must have been initialized and the PXE protocol
 *      carried out in order to discover and download this program.
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
#define OPT4_PAD 0

/*
 * Option 12: Hostname
 *
 * This option specifies the name of the client.  The name may or may
 * not be qualified with the local domain name (RFC 1533 see section 3.17 for
 * the preferred way to retrieve the domain name).  See RFC 1035 for
 * character set restrictions.
 */
#define OPT4_HOSTNAME 12

/*
 * Option 15: Domain
 *
 * This option specifies the domain name that client should use when
 * resolving hostnames via the Domain Name System.
 */
#define OPT4_DOMAIN 15

/*
 * Option 54: Server Identifier
 *
 * "DHCP clients use the contents of the 'server identifier' field as the
 * destination address for any DHCP messages unicast to the DHCP
 * server. ... The identifier is the IP address of the selected server."
 */
#define OPT4_SERVER_IDENTIFIER 54

/*
 * Option 67: boot-file
 *
 * "This string is the path for the boot file. It MUST comply with STD 66
 *  [RFC3986]. The string is not NUL-terminated."
 */
#define OPT4_BOOTFILE_PATH 67

/*
 * Option 255: End Option
 *
 * "The end option marks the end of valid information in the vendor field.
 * Subsequent octets should be filled with pad options.  The code for the end
 * option is 255, and its length is 1 octet."
 */
#define OPT4_END 255

/*
 * Standard DHCPv6 options
 *
 * Option 39: Client FQDN
 *   To update the IPv6-address-to-FQDN mapping, a DHCPv6 server needs to
 *   know the FQDN of the client for the addresses for the client's IA_NA
 *   bindings.  To allow the client to convey its FQDN to the server, this
 *   document defines a new DHCPv6 option called "Client FQDN".  The
 *   Client FQDN option also contains Flags that DHCPv6 clients and
 *   servers use to negotiate who does which updates.
 */
#define OPT6_FQDN 39

/*
 * Standard DHCPv6 options
 *
 * Option 59: boot-file-url
 *   This string is the URL for the boot file. It MUST comply with STD 66
 *   [RFC3986]. The string is not NUL-terminated.
 */
#define OPT6_BOOTFILE_URL 59

/*
 * RFC3986 specifies that URLs should not be longer than 255 characters.
 * Let's tolerate more, in case a DHCP server chose to ignore that.
 */
#define URL_SIZE_MAX 1024

/*
 * Maximum supported boot file length that can be specified with option
 * 67 is 255 characters as per PXE standards and 1 byte for NUL.
 */
#define BOOTFILE_SIZE_MAX 256

static bool isIPv6 = false;

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
   optEnd = (uint8_t *)Packet + sizeof (EFI_PXE_BASE_CODE_PACKET);

   while (p < optEnd) {
      optCode = ntohs(*(uint16_t *)p);
      p += sizeof(optCode);

      optLen = ntohs(*(uint16_t *)p);
      p += sizeof(optLen);

      // Protect from bogus/malicious length in DHCP option packets
      optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);

      if (optCode == OPT6_BOOTFILE_URL) {
         // Per RFC3986, option string is not NUL-terminated.
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

/*-- get_ipv4_boot_file -------------------------------------------------------
 *
 *      Retrieve the IPv4 boot file from a PXE option 67. PXE BC packet is
 *      parsed to retrive the boot file path.
 *
 * Parameters
 *      IN Packet:    pointer to a PXE BC packet
 *      OUT bootfile: Full path for boot file configured using option 67.
 *
 * Results
 *      Valid bootfile on success else NULL.
 *----------------------------------------------------------------------------*/
static const char *get_ipv4_boot_file(const EFI_PXE_BASE_CODE_PACKET *Packet,
                                      char bootFile[BOOTFILE_SIZE_MAX])
{
   const uint8_t *p;
   const uint8_t *optEnd;
   uint8_t optCode, optLen;

   p = (uint8_t *)Packet->Dhcpv4.DhcpOptions;
   optEnd = (uint8_t *)Packet + sizeof (EFI_PXE_BASE_CODE_PACKET);

   while (p < optEnd) {
      optCode = *p++;

      if (optCode == OPT4_PAD) {
         continue;
      }
      if (optCode == OPT4_END) {
         break;
      }

      optLen = *p++;

      // Protect from bogus/malicious length in DHCP option packets
      optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);
      if (optCode == OPT4_BOOTFILE_PATH) {
         // Per RFC3986, option string is not NUL-terminated.
         optLen = MIN(optLen, BOOTFILE_SIZE_MAX - 1);
         memcpy(bootFile, p, optLen);
         bootFile[optLen] = '\0';
         return bootFile;
      }

      p += optLen;
   }

   return NULL;
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
   optEnd = (uint8_t *)Packet + sizeof (EFI_PXE_BASE_CODE_PACKET);

   while (p < optEnd) {
      optCode = *p++;

      if (optCode == OPT4_PAD) {
         continue;
      }
      if (optCode == OPT4_END) {
         break;
      }

      optLen = *p++;

      // Protect from bogus/malicious length in DHCP option packets
      optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);

      if (optCode == OPT4_SERVER_IDENTIFIER &&
          optLen == sizeof(ServerIp->v4)) {

         memcpy(ServerIp, p, optLen);
         return EFI_SUCCESS;
      }

      p += optLen;
   }

   memset(ServerIp, 0, sizeof(*ServerIp));
   return EFI_NOT_FOUND;
}

/*-- get_ipv4_hostname ---------------------------------------------------------
 *
 *      Retrieve the IPv4 hostname from the DHCP options cached in the PXE BC
 *      packet.
 *
 * Parameters
 *      IN Packet:    pointer to a PXE BC packet. An EFI_INVALID_PARAMETER will
 *                    be returned if this is NULL.
 *      OUT hostname: the hostname from DHCPv4 option 12 if available. The
 *                    'hostname' parameter is expected to point to a pointer
 *                    which points to NULL intially and if this is not the case
 *                    an EFI_INVALID_PARAMETER will be returned.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status. If the pointer to which the
 *      'hostname' output parameter is pointing is not NULL after this function
 *      is invoked the pointer should be freed even if an error is reported.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_ipv4_hostname(const EFI_PXE_BASE_CODE_PACKET *Packet,
                                    char **hostname)
{
   EFI_STATUS status = EFI_NOT_FOUND;
   const uint8_t *p;
   const uint8_t *optEnd;
   uint8_t optCode, optLen;

   if (Packet == NULL || hostname == NULL || *hostname != NULL) {
      return EFI_INVALID_PARAMETER;
   }

   p = (uint8_t *)Packet->Dhcpv4.DhcpOptions;
   optEnd = (uint8_t *)Packet + sizeof (EFI_PXE_BASE_CODE_PACKET);

   while (p < optEnd) {
      optCode = *p++;

      if (optCode == OPT4_PAD) {
         continue;
      }
      if (optCode == OPT4_END) {
         break;
      }

      optLen = *p++;

      // Protect from bogus/malicious length in DHCP option packets
      optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);

      if (optCode == OPT4_HOSTNAME) {
         if ((status = str_alloc(optLen, hostname)) == EFI_SUCCESS) {
            memcpy(*hostname, p, optLen);
            (*hostname)[optLen] = '\0';
         }
         break;
      }

      p += optLen;
   }

   return status;
}

/*-- get_ipv4_domain -----------------------------------------------------------
 *
 *      Retrieve the domian from the DHCP options cached in the PXE BC packet.
 *
 * Parameters
 *      IN Packet:  pointer to a PXE BC packet. An EFI_INVALID_PARAMETER will be
 *                  returned if this parameter is NULL.
 *      OUT domain: the domain from DHCPv4 option 15 if available. The
 *                  'domain' parameter is expected to point to a pointer which
 *                  points to NULL intially and if this is not the case an
 *                  EFI_INVALID_PARAMETER will be returned.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status. If the pointer to which the
 *      'domain' output parameter is pointing is not NULL after this function
 *      is invoked the pointer should be freed even if an error is reported.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_ipv4_domain(const EFI_PXE_BASE_CODE_PACKET *Packet,
                                  char **domain)
{
   EFI_STATUS status = EFI_NOT_FOUND;
   const uint8_t *p;
   const uint8_t *optEnd;
   uint8_t optCode, optLen;

   if (Packet == NULL || domain == NULL || *domain != NULL) {
      return EFI_INVALID_PARAMETER;
   }

   p = (uint8_t *)Packet->Dhcpv4.DhcpOptions;
   optEnd = (uint8_t *)Packet + sizeof (EFI_PXE_BASE_CODE_PACKET);

   while (p < optEnd) {
      optCode = *p++;

      if (optCode == OPT4_PAD) {
         continue;
      }
      if (optCode == OPT4_END) {
         break;
      }

      optLen = *p++;

      // Protect from bogus/malicious length in DHCP option packets
      optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);

      if (optCode == OPT4_DOMAIN) {
         if ((status = str_alloc(optLen, domain)) == EFI_SUCCESS) {
            memcpy(*domain, p, optLen);
            (*domain)[optLen] = '\0';
         }
         break;
      }

      p += optLen;
   }

   return status;
}

/*-- get_ipv6_client_fqdn -------------------------------------------------------------
 *
 *      Retrieve the client FQDN from the DHCP options cached in the PXE BC packet.
 *
 * Parameters
 *      IN Packet:  pointer to a PXE BC packet. An EFI_INVALID_PARAMETER will be
 *                  returned if this parameter is NULL.
 *      OUT fqdn: the FQDN from DHCPv6 option 39 if available. The
 *                'fqdn' parameter is expected to point to a pointer which
 *                points to NULL intially and if this is not the case an
 *                EFI_INVALID_PARAMETER will be returned.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status. If the pointer to which the
 *      'fqdn' output parameter is pointing is not NULL after this function
 *      is invoked the pointer should be freed even if an error is reported.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_ipv6_client_fqdn(const EFI_PXE_BASE_CODE_PACKET *Packet,
                                       char **fqdn)
{
   EFI_STATUS status = EFI_NOT_FOUND;
   const uint8_t *p;
   const uint8_t *optEnd;
   uint16_t optCode, optLen;

   if (Packet == NULL || fqdn == NULL || *fqdn != NULL) {
      return EFI_INVALID_PARAMETER;
   }

   p = (uint8_t *)Packet->Dhcpv6.DhcpOptions;
   optEnd = (uint8_t *)Packet + sizeof (EFI_PXE_BASE_CODE_PACKET);

   while (p < optEnd) {
      optCode = ntohs(*(uint16_t *)p);
      p += sizeof(optCode);

      optLen = ntohs(*(uint16_t *)p);
      p += sizeof(optLen);

      if (optCode == OPT6_FQDN) {
         // Skip 'flags' field
         p++;

         // Protect from bogus/malicious length in DHCP option packets
         optLen = MIN(optLen, (uintptr_t)optEnd - (uintptr_t)p);

         // Per RFC4704, option string is not NUL-terminated.
         if ((status = str_alloc(optLen, fqdn)) == EFI_SUCCESS) {
            memcpy(*fqdn, p, optLen);
            (*fqdn)[optLen] = '\0';
         }
         break;
      }

      p += optLen;
   }

   return status;
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
EFI_STATUS get_pxe_info(EFI_PXE_BASE_CODE **Pxe,
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

   memset(ServerIp, 0, sizeof(*ServerIp));
   if (isIPv6) {
      Status = get_tftp_ipv6_addr(Packet, ServerIp);
      if (EFI_ERROR(Status)) {
         return Status;
      }

      if (memcmp(&lastServerIp, ServerIp, sizeof(*ServerIp)) != 0) {
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

      if (memcmp(&lastServerIp, ServerIp, sizeof(*ServerIp)) != 0) {
         Log(LOG_DEBUG, "tftp4 server IP %u.%u.%u.%u",
             ServerIp->v4.Addr[0], ServerIp->v4.Addr[1],
             ServerIp->v4.Addr[2], ServerIp->v4.Addr[3]);
      }
   }
   memcpy(&lastServerIp, ServerIp, sizeof(*ServerIp));

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
      char file[BOOTFILE_SIZE_MAX];
      *BootFile = NULL;

      /*
       * Boot file can be passed by DHCP option 67 for ipv4 cases which takes
       * precedence over boot file in DHCP header.
       */
      if (get_ipv4_boot_file(Packet, file) != NULL) {
         return ascii_to_ucs2(file, BootFile);
      }

      return ascii_to_ucs2(((char *)Packet->Dhcpv4.BootpBootFile), BootFile);
   }
}

/*-- get_pxe_boot_hostname_and_domain ------------------------------------------
 *
 *      For IPv4 retrieve the client hostname and domain from the DHCP options
 *      cached in the PXE BC packet. For IPv6 deduce the client hostname and
 *      domain from the client FQDN DHCP option cached in the PXE BC packet.
 *
 * Parameters
 *      IN Packet:    pointer to a PXE BC packet. An EFI_INVALID_PARAMETER will
 *                    be returned if this is NULL.
 *      OUT hostname: the hostname from DHCPv4 option 12 if available. The
 *                    'hostname' parameter is expected to point to a pointer
 *                    which points to NULL intially and if this is not the case
 *                    an EFI_INVALID_PARAMETER will be returned.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status. If the pointer to which the
 *      'hostname' output parameter is pointing is not NULL after this function
 *      is invoked the pointer should be freed even if an error is reported.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_pxe_boot_hostname_and_domain(
   EFI_PXE_BASE_CODE *Pxe,
   const char **Hostname,
   const char **Domain)
{
   EFI_STATUS Status;
   EFI_PXE_BASE_CODE_PACKET *Packet = NULL;
   char *local_hostname = NULL;
   char *local_domain = NULL;

   EFI_ASSERT_PARAM(Pxe != NULL);
   EFI_ASSERT_PARAM(Hostname != NULL && *Hostname == NULL);
   EFI_ASSERT_PARAM(Domain != NULL && *Domain == NULL);

   Status = get_pxe_base_code_packet(Pxe, &Packet);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Failed to obtain PXE Boot Code Packet.");
      goto out;
   }

   if (isIPv6) {
      Status = get_ipv6_client_fqdn(Packet, &local_hostname);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Failed to obtain DHCPv6 FQDN.");
         goto out;
      }
   
      local_domain = strchr(local_hostname, '.');
      if (local_domain != NULL) {
         *local_domain++ = '\0';
      }
   } else {
      Status = get_ipv4_hostname(Packet, &local_hostname);
      if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
         Log(LOG_ERR, "Failed to obtain DHCPv4 HOSTNAME.");
         goto out;
      }

      Status = get_ipv4_domain(Packet, &local_domain);
      if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
         Log(LOG_ERR, "Failed to obtain DHCPv4 DOMAIN.");
         goto out;
      }
   }

   if (local_hostname != NULL) {
      *Hostname = strdup(local_hostname);
   }

   if (local_domain != NULL) {
      *Domain = strdup(local_domain);
   }

out:

   if (!isIPv6) {
      free(local_domain);
      local_domain = NULL;
   }
   free(local_hostname);
   local_hostname = NULL;

   return Status;
}
