/*******************************************************************************
 * Copyright (c) 2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * ishttp.c -- Check if current image was loaded via HTTP.
 * (Split from httpfile.c.)
 */

#include "efi_private.h"
#include "Http.h"

static char *httpBootUrl;

/*-- is_http_boot --------------------------------------------------------------
 *
 *      Check whether the current running image was loaded directly via HTTP.
 *      "Directly" means the HTTP URL was for the image itself, not for a
 *      ramdisk containing the image.
 *
 * Results
 *      True/false
 *
 * Side effects
 *      Cache information about the image.
 *----------------------------------------------------------------------------*/
bool is_http_boot(void)
{
   static bool cached = false;
   static bool is_http = false;
   EFI_STATUS Status;
   EFI_HANDLE Volume;
   EFI_DEVICE_PATH *VolumePath;
   EFI_DEVICE_PATH *node;

   if (cached) {
      return is_http;
   }
   cached = true;

   /*
    * NOTE: When DEBUG is not defined at build time, this function is typically
    * first called before logging has been initialized, so the Log calls in it
    * have no effect.  Even if DEBUG is defined, serial is not yet initialized,
    * so the Log calls will show up on serial only if firmware is directing
    * output to serial.  (The call chain is efi_main -> efi_create_argv ->
    * get_boot_file.)
    */

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "EFI error getting boot volume: %zx", Status);
      return false;
   }
   Status = devpath_get(Volume, &VolumePath);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting boot volume devpath: %zx", Status);
      return false;
   }
   log_devpath(LOG_DEBUG, "boot volume", VolumePath);

   FOREACH_DEVPATH_NODE(VolumePath, node) {
      if (node->Type == MESSAGING_DEVICE_PATH &&
          node->SubType == MSG_URI_DP) {
         size_t len;
         char *url;

         /* Save a copy of the URL for get_http_boot_url */
         len = DevPathNodeLength(node) - sizeof(*node);
         url = sys_malloc(len + 1);
         memcpy(url, &((URI_DEVICE_PATH *)node)->Uri, len);
         url[len] = '\0';
         httpBootUrl = url;
         is_http = true;

      } else if (node->Type == MEDIA_DEVICE_PATH &&
                 node->SubType == MEDIA_RAM_DISK_DP) {
         /* Ramdisk node found: not direct HTTP boot */
         is_http = false;
         break;
      }
   }

   if (httpBootUrl == NULL) {
      Log(LOG_DEBUG, "Image not loaded via UEFI HTTP");
   } else {
      Log(LOG_DEBUG, "Image loaded %s via UEFI HTTP, URL %s",
          is_http ? "directly" : "from ramdisk", httpBootUrl);
   }

   return is_http;
}

/*-- get_http_boot_url ---------------------------------------------------------
 *
 *      Get the URL of the HTTP boot file.
 *
 * Parameter
 *      OUT buffer: pointer to the freshly allocated URL.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_http_boot_url(char **buffer)
{
   if (is_http_boot()) {
      *buffer = strdup(httpBootUrl);
      return ERR_SUCCESS;
   } else {
      return ERR_UNSUPPORTED;
   }
}

