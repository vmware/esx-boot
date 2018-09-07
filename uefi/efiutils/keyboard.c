/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * keyboard.c -- Basic keyboard handling.
 */

#include "efi_private.h"

typedef struct {
   uint16_t scancode;   /* EFI-specific scancode */
   char ascii;          /* ASCII value (0 if no ASCII equivalent) */
   key_sym_t sym;       /* Abstracted key symbol */
} scan_code_t;

static const scan_code_t scancodes[] = {
   {0x01, 0x00, KEYSYM_UP},
   {0x02, 0x00, KEYSYM_DOWN},
   {0x03, 0x00, KEYSYM_RIGHT},
   {0x04, 0x00, KEYSYM_LEFT},
   {0x05, 0x00, KEYSYM_HOME},
   {0x06, 0x00, KEYSYM_END},
   {0x07, 0x00, KEYSYM_INSERT},
   {0x08, 0x7f, KEYSYM_ASCII},      /* [DELETE] */
   {0x09, 0x00, KEYSYM_PAGE_UP},
   {0x0a, 0x00, KEYSYM_PAGE_DOWN},
   {0x0b, 0x00, KEYSYM_F1},
   {0x0c, 0x00, KEYSYM_F2},
   {0x0d, 0x00, KEYSYM_F3},
   {0x0e, 0x00, KEYSYM_F4},
   {0x0f, 0x00, KEYSYM_F5},
   {0x10, 0x00, KEYSYM_F6},
   {0x11, 0x00, KEYSYM_F7},
   {0x12, 0x00, KEYSYM_F8},
   {0x13, 0x00, KEYSYM_F9},
   {0x14, 0x00, KEYSYM_F10},
   {0x15, 0x00, KEYSYM_F11},
   {0x16, 0x00, KEYSYM_F12},
   {0x17, 0x1b, KEYSYM_ASCII}       /* [ESCAPE] */
};

/*-- kbd_getkey ----------------------------------------------------------------
 *
 *      Get the next keystroke if it represent a valid KEYSYM (defined in
 *      boot_services.h) or an ASCII character. All other key sequences are
 *      ignored. This function is non blocking: if no keystroke is available,
 *      kbd_getkey() returns successfully and key->sym is set to KEYSYM_NONE.
 *
 * Parameters
 *      IN key: pointer to a keystroke info structure that is set as follow:
 *               - no keystroke: key->sym is set to KEYSYM_NONE
 *               - a keystroke represents a Unicode character: key->sym is set
 *                 to KEYSYM_UNICODE and key->unicode contains the character
 *               - special keystroke: key->sym contains the key symbol
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int kbd_getkey(key_code_t *key)
{
   EFI_INPUT_KEY Key;
   EFI_STATUS Status;
   size_t i;

   Status = st->ConIn->ReadKeyStroke(st->ConIn, &Key);

   if (Status == EFI_NOT_READY) {
      key->sym = KEYSYM_NONE;
      key->ascii = 0;
      Status = EFI_SUCCESS;
   } else if (!EFI_ERROR(Status)) {
      key->sym = KEYSYM_NONE;
      key->ascii = 0;

      /*
       * UEFI Specification says: "If the Scan Code is set to 0x00 then the
       * Unicode character is valid and should be used. If the Scan Code is
       * set to a non-0x00 value it represents a special key."
       */
      if (Key.ScanCode != 0x00) {
         for (i = 0; i < ARRAYSIZE(scancodes); i++) {
            if (scancodes[i].scancode == Key.ScanCode) {
               key->sym = scancodes[i].sym;
               key->ascii = scancodes[i].ascii;
               break;
            }
         }
      } else {
         /*
          * UEFI EFI Specification says: "characters and strings are stored
          * in the UCS-2 encoding format as defined by Unicode 2.1 and ISO_IEC
          * 10646 standards."
          */
         if (Key.UnicodeChar < 256) {
            key->sym = KEYSYM_ASCII;
            key->ascii = (char)Key.UnicodeChar;
         }
      }
   }

   return error_efi_to_generic(Status);
}

/*-- kbd_waitkey ---------------------------------------------------------------
 *
 *      Get the next keystroke if it represent a valid KEYSYM (defined in
 *      boot_services.h) or an ASCII character. All other key sequences are
 *      ignored. This function blocks until a key is pressed.
 *
 *      NOTE: The UEFI watchdog timer is disabled until a keyboard event occurs,
 *
 * Parameters
 *      IN key: pointer to a keystroke info structure that is set as follow:
 *               - a keystroke represents a Unicode character: key->sym is set
 *                 to KEYSYM_UNICODE and key->unicode contains the character
 *               - special keystroke: key->sym contains the key symbol
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int kbd_waitkey(key_code_t *key)
{
   UINTN Index;
   EFI_STATUS Status;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->WaitForEvent != NULL);
   EFI_ASSERT(st != NULL);
   EFI_ASSERT_FIRMWARE(st->ConIn != NULL);
   EFI_ASSERT_FIRMWARE(st->ConIn->WaitForKey != NULL);

   efi_set_watchdog_timer(WATCHDOG_DISABLE);
   Status = bs->WaitForEvent(1, &st->ConIn->WaitForKey, &Index);
   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   EFI_ASSERT_FIRMWARE(Index == 0);

   return kbd_getkey(key);
}

/*-- kbd_waitkey_timeout -------------------------------------------------------
 *
 *      Get the next keystroke if it represent a valid KEYSYM (defined in
 *      boot_services.h) or an ASCII character. All other key sequences are
 *      ignored. This function blocks until a key is pressed or the timeout
 *      expires.
 *
 *      NOTE: The UEFI watchdog timer is disabled until a keyboard event occurs,
 *            or the timer timeouts.
 *
 * Parameters
 *      IN key: pointer to a keystroke info structure that is set as follow:
 *               - a keystroke represents a Unicode character: key->sym is set
 *                 to KEYSYM_UNICODE and key->unicode contains the character
 *               - special keystroke: key->sym contains the key symbol
 *      IN nsec: timeout in seconds
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int kbd_waitkey_timeout(key_code_t *key, uint16_t nsec)
{
   UINTN Index;
   EFI_STATUS Status;
   EFI_EVENT Timer, Events[2];

   if (nsec == 0) {
      return kbd_getkey(key);
   }

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->WaitForEvent != NULL);
   EFI_ASSERT_FIRMWARE(bs->CreateEvent != NULL);
   EFI_ASSERT_FIRMWARE(bs->CloseEvent != NULL);
   EFI_ASSERT_FIRMWARE(bs->SetTimer != NULL);
   EFI_ASSERT(st != NULL);
   EFI_ASSERT_FIRMWARE(st->ConIn != NULL);
   EFI_ASSERT_FIRMWARE(st->ConIn->WaitForKey != NULL);

   Status = bs->CreateEvent(EVT_TIMER, 0, NULL, NULL, &Timer);
   if (EFI_ERROR (Status)) {
      return Status;
   }

   Status = bs->SetTimer(Timer, TimerRelative, (UINT64)nsec * 10000000);
   if (EFI_ERROR(Status)) {
      bs->CloseEvent(Timer);
      return Status;
   }

   Events[0] = st->ConIn->WaitForKey;
   Events[1] = Timer;

   efi_set_watchdog_timer(WATCHDOG_DISABLE);
   Status = bs->WaitForEvent(2, Events, &Index);
   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   bs->CloseEvent(Timer);

   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (Index == 0) {
      return kbd_getkey(key);
   } else {
      key->sym = KEYSYM_NONE;
      key->ascii = 0;
      return error_efi_to_generic(EFI_SUCCESS);
   }
}

/*-- kbd_init ------------------------------------------------------------------
 *
 *      Check/initialize the keyboard hardware and clear the keystroke buffer.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int kbd_init(void)
{
   EFI_STATUS Status;

   EFI_ASSERT(st != NULL);
   EFI_ASSERT_FIRMWARE(st->ConIn != NULL);
   EFI_ASSERT_FIRMWARE(st->ConIn->Reset != NULL);

   Status = st->ConIn->Reset(st->ConIn, FALSE);

   return error_efi_to_generic(Status);
}
