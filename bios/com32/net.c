/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * net.c -- Network-related COM32 functions
 */

#include <boot_services.h>
#include "com32_private.h"

#define PXENV_EXIT_SUCCESS            0
#define PXENV_STATUS_SUCCESS          0
#define PXENV_FILE_API_CHECK          0xe6
#define PXENV_FILE_API_MAGIC_INBOUND  0x91d447b2
#define PXENV_FILE_API_MAGIC_OUTBOUND 0xe9c17b20
#define PXENV_FILE_API_EXEC_SUPPORT   (1 << 5)

#pragma pack(1)
struct s_PXENV_FILE_CHECK_API {
   uint16_t Status;
   uint16_t Size;
   uint32_t Magic;
   uint32_t Provider;
   uint32_t APIMask;
   uint32_t Flags;
};
#pragma pack()

/*-- com32_call_pxe_stack ------------------------------------------------------
 *
 *      Wrapper for the 'Call PXE Stack' COM32 service.
 *
 * Parameters
 *      IN func:   PXE function number
 *      IN buffer: PXE parameter structure buffer
 *      IN buflen: PXE parameter structure buffer size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_call_pxe_stack(uint16_t func, void *buffer, size_t buflen)
{
   com32sys_t iregs, oregs;
   farptr_t fptr;
   void *buf;
   int status;

   if (buflen > get_bounce_buffer_size()) {
      return ERR_INVALID_PARAMETER;
   }

   buf = get_bounce_buffer();
   memcpy(buf, buffer, buflen);

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(buf);
   iregs.eax.w[0] = 0x09;
   iregs.ebx.w[0] = func;
   iregs.es = fptr.real.segment;
   iregs.edi.w[0] = fptr.real.offset;
   status = intcall_check_CF(COM32_INT, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (oregs.eax.w[0] != PXENV_EXIT_SUCCESS) {
      return ERR_DEVICE_ERROR;
   }

   memcpy(buffer, buf, buflen);

   return ERR_SUCCESS;
}

/*-- isGPXE ---------------------------------------------------------
 *
 *      Check whether we are talking to gPXE.
 *
 * Results
 *      True if gPXE, false otherwhise.
 *----------------------------------------------------------------------------*/
bool isGPXE(void)
{
   struct s_PXENV_FILE_CHECK_API fca;
   int status;

   if (com32.derivative == COM32_DERIVATIVE_GPXE) {
      return true;
   } else if (com32.derivative != COM32_DERIVATIVE_PXELINUX) {
      return false;
   }

   memset(&fca, 0, sizeof (fca));
   fca.Size = sizeof (fca);
   fca.Magic = PXENV_FILE_API_MAGIC_INBOUND;

   status = com32_call_pxe_stack(PXENV_FILE_API_CHECK, &fca, sizeof (fca));
   if (status != ERR_SUCCESS) {
      return false;
   }

   return ((fca.Status == PXENV_STATUS_SUCCESS) &&
           (fca.Magic == PXENV_FILE_API_MAGIC_OUTBOUND) &&
           (fca.Size >= sizeof (fca)) &&
           ((fca.APIMask & PXENV_FILE_API_EXEC_SUPPORT) != 0));
}

/*-- is_network_boot -----------------------------------------------------------
 *
 *      Check whether we are booted from the network (PXE or gPXE).
 *
 * Results
 *      True/false
 *----------------------------------------------------------------------------*/
bool is_network_boot(void)
{
   return (isPxelinux() || isGPXE());
}

/*-- com32_get_ipappend --------------------------------------------------------
 *
 *      Wrapper for the 'Get IPAPPEND strings' COM32 service.
 *
 * Parameters
 *      OUT ip:     pointer to the "ip=" string
 *      OUT bootif: pointer to the "bootif=" string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_get_ipappend(const char **ip, const char **bootif)
{
   const uint16_t *ipappend;
   com32sys_t iregs, oregs;
   const char *iface;
   farptr_t fptr;
   int status;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x0f;
   status = intcall_check_CF(COM32_INT, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (oregs.ecx.w[0] < 2) {
      return ERR_UNSUPPORTED;
   }

   fptr.real.segment = oregs.es;
   fptr.real.offset = oregs.ebx.w[0];
   ipappend = real_to_virtual(fptr);
   if (ipappend == NULL) {
      return ERR_UNSUPPORTED;
   }

   fptr.real.offset = ipappend[1];
   iface = real_to_virtual(fptr);
   if (strncmp("BOOTIF=", iface, 7) != 0) {
      return ERR_UNSUPPORTED;
   }

   fptr.real.offset = ipappend[0];
   *ip = real_to_virtual(fptr);
   *bootif = iface;

   return ERR_SUCCESS;
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
 *      NOTE: Unfortunately, gPXE does not support providing the boot interface
 *            mac address (unless it is chainloaded by pxelinux). As a result,
 *            the BOOTIF= option still has to be passed manually (via <SHIFT+O>,
 *            or by editing the boot.cfg configuration file) when booting from
 *            gPXE.
 *
 * Parameters
 *      OUT bootif: a pointer to the statical BOOTIF string.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_bootif_option(const char **bootif)
{
   const char *ip;

   return com32_get_ipappend(&ip, bootif);
}
