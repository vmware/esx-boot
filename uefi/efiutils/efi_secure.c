/*******************************************************************************
 * Copyright (c) 2015,2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * efi_secure.c -- Support for UEFI Secure Boot
 */
#include <stdbool.h>
#include <efiutils.h>
#include <bootlib.h>

/*-- secure_boot_mode ----------------------------------------------------------
 *
 *      Is the platform firmware operating in Secure Boot mode?  From
 *      the UEFI 2.5 spec: "The platform firmware is operating in
 *      secure boot mode if the value of the SetupMode variable is 0
 *      and the SecureBoot variable is set to 1."
 *
 * Results
 *      true if booting in Secure Boot mode; false if not
 *----------------------------------------------------------------------------*/
bool secure_boot_mode(void)
{
   UINTN DataSize;
   EFI_STATUS Status;
   UINT8 SetupMode, SecureBoot;
   /*
    * These values are in fact not modified by GetVariable, but they
    * can't be const because UEFI doesn't declare IN parameters to be
    * const.  Sigh.
    */
   static CHAR16 SetupModeName[] = L"SetupMode";
   static CHAR16 SecureBootName[] = L"SecureBoot";
   static EFI_GUID EfiGlobalVariable =
      { 0x8BE4DF61,0x93CA,0x11d2,{0xAA,0x0D,0x00,0xE0,0x98,0x03,0x2B,0x8C} };

   DataSize = sizeof(SetupMode);
   Status = rs->GetVariable(SetupModeName, &EfiGlobalVariable,
                            NULL, &DataSize, &SetupMode);
   if (Status != EFI_SUCCESS || DataSize != sizeof(SetupMode)) {
      Log(LOG_DEBUG, "Failed to read SetupMode variable: 0x%zx", Status);
      return false;
   }

   DataSize = sizeof(SecureBoot);
   Status = rs->GetVariable(SecureBootName, &EfiGlobalVariable,
                            NULL, &DataSize, &SecureBoot);
   if (Status != EFI_SUCCESS || DataSize != sizeof(SecureBoot)) {
      Log(LOG_DEBUG, "Failed to read SecureBoot variable: 0x%zx", Status);
      return false;
   }

   Log(LOG_DEBUG, "SetupMode = %u, SecureBoot = %u",
       SetupMode, SecureBoot);

   return !SetupMode && SecureBoot;
}
