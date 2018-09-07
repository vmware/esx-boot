/*******************************************************************************
 * Copyright (c) 2008-2013 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * net.c -- Network related functions
 */

#include <boot_services.h>
#include <bootlib.h>

/*-- get_mac_address -----------------------------------------------------------
 *
 *      The result is a string of the form "xx-aa-bb-cc-dd-ee-ff", where xx is
 *      the Hardware Type Number of the boot interface (see RFC 1700), and
 *      aa:bb:cc:dd:ee:ff is its MAC address.
 *
 * Parameters
 *      OUT mac: a pointer to the MAC address part of the statical BOOTIF
 *               string.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_mac_address(const char **mac)
{
   const char *bootif;
   int status;

   status = get_bootif_option(&bootif);
   if (status != ERR_SUCCESS) {
      return status;
   }

   /* get_bootif_option() returns the same string as the pxelinux "ipappend 2"
    * option would have added to the command line.
    * The string is of the form "BOOTIF=xx-aa-bb-cc-dd-ee-ff", where xx and
    * aa:bb:cc:dd:ee:ff have the same meaning as described in the function
    * header.
    */
   *mac = bootif + 7;

   return ERR_SUCCESS;
}
