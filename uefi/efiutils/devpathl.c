/*******************************************************************************
 * Copyright (c) 2008-2011,2016,2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include <string.h>
#include "efi_private.h"

/*-- log_devpath ---------------------------------------------------------------
 *
 *      Convert a device path to text and log it.
 *
 * Parameters
 *      IN  level:   log level
 *      IN  prefix:  string to prefix the device path with
 *      IN  DevPath: device path
 *----------------------------------------------------------------------------*/
void log_devpath(int level, const char *prefix, const EFI_DEVICE_PATH *DevPath)
{
   char *text;

   text = devpath_text(DevPath, false, false);
   Log(level, "%s: %s", prefix, text);
   sys_free(text);
}

/*-- log_handle_devpath --------------------------------------------------------
 *
 *      Get the device path associated with an EFI handle, convert it to text,
 *      and log it.
 *
 * Parameters
 *      IN  level:  log level
 *      IN  prefix: string to prefix the device path with
 *      IN  handle: EFI handle
 *----------------------------------------------------------------------------*/
void log_handle_devpath(int level, const char *prefix, EFI_HANDLE handle)
{
   EFI_STATUS Status;
   EFI_DEVICE_PATH *DevPath;

   Status = devpath_get(handle, &DevPath);
   if (EFI_ERROR(Status)) {
      Log(level, "%s: EFI error getting devpath: %zx", prefix, Status);
      return;
   }

   log_devpath(level, prefix, DevPath);
}
