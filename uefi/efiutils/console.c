/*******************************************************************************
 * Copyright (c) 2008-2012,2014-2015,2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * console.c -- EFI console management
 */

#include <stdio.h>
#include <stdarg.h>
#include <io.h>

#include "efi_private.h"

#define EFI_MESSAGE_BUFLEN   128

static EFI_GUID ConsoleControlProto = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

/*-- set_graphic_mode ----------------------------------------------------------
 *
 *      Enable graphic mode.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int set_graphic_mode(void)
{
   EFI_CONSOLE_CONTROL_PROTOCOL *console;
   EFI_CONSOLE_CONTROL_SCREEN_MODE current_mode;
   EFI_STATUS Status;

   Status = LocateProtocol(&ConsoleControlProto, (void **)&console);
   if (EFI_ERROR(Status)) {
      /*
       * Most of UEFI 2.0 firmwares do not have a ConsoleControlProtocol, and
       * the hardware should already be in a linear graphic mode.
       * (EDK II has the GraphicsConsole and ConSplitter modules that assume
       * this in this case).
       * Therefore, we return success here and hope for the best.
       */
      return ERR_SUCCESS;
   }

   console->GetMode(console, &current_mode, NULL, NULL);
   if (current_mode != EfiConsoleControlScreenGraphics) {
      console->SetMode(console, EfiConsoleControlScreenGraphics);
   }

   return ERR_SUCCESS;
}

/*-- efi_print -----------------------------------------------------------------
 *
 *      Print an UEFI string on the default 'ConOut' device.
 *
 * Parameters
 *      IN Str: pointer to the UCS-2 string to print
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS efi_print(CHAR16 *Str)
{
   if (Str == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if (st == NULL || st->ConOut == NULL || st->ConOut->OutputString == NULL) {
      return EFI_UNSUPPORTED;
   }

   return st->ConOut->OutputString(st->ConOut, Str);
}

/*-- firmware_print ------------------------------------------------------------
 *
 *      Generic wrapper for efi_print() to print a standard ASCII string. The
 *      string is first converted to the UCS-2 format, and '\n' are replaced
 *      the "\n\r" sequence.
 *
 * Parameters
 *      IN str: pointer to the ASCII string to print
 *
 * Results
 *      A generic error code.
 *----------------------------------------------------------------------------*/
int firmware_print(const char *str)
{
   CHAR16 Buffer[EFI_MESSAGE_BUFLEN];
   CHAR16 *Str;
   char *buf;
   EFI_STATUS Status;
   int i, j;

   Str = &Buffer[0];
   buf = (char *)Str;
   i = 0;

   while (str[i] != '\0'){
      for (j = 0; j < (EFI_MESSAGE_BUFLEN - 2); i++) {
         buf[j++] = str[i];

         if (str[i] == '\n') {
            buf[j++] = '\r';
         } else if (str[i] == '\0') {
            break;
         }
      }

      if (j > 0 && buf[j - 1] != '\0') {
         buf[j] = '\0';
      }

      ascii_to_ucs2(buf, &Str);

      Status = efi_print(Str);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }
   }

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- efi_assert ----------------------------------------------------------------
 *
 *      Assert routine (debugging purposes)
 *
 * Parameters
 *      IN msg:   pointer to a message to be displayed
 *      IN ...:   arguments list for the message format
 *----------------------------------------------------------------------------*/
#ifdef DEBUG
void efi_assert(const char *msg, ...)
{
   char buffer[EFI_MESSAGE_BUFLEN];
   va_list ap;

   va_start(ap, msg);
   vsnprintf(buffer, EFI_MESSAGE_BUFLEN, msg, ap);
   va_end(ap);

   Log(LOG_EMERG, "%s", buffer);
}
#endif
