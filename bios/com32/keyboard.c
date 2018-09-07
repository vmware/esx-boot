/*******************************************************************************
 * Copyright (c) 2008-2011,2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * keyboard.c -- Basic keyboard handling
 */

#include <string.h>
#include <bios.h>
#include <cpu.h>

#include "com32_private.h"

#define KEY_NONE (-1)

typedef struct {
   int scancode;        /* COM32-specific scancode */
   char ascii;          /* ASCII value (0 if no ASCII equivalent) */
   key_sym_t sym;       /* Abstracted key symbol */
} scan_code_t;

static const scan_code_t scancodes[] = {
   {0x48, 0x00, KEYSYM_UP},
   {0x50, 0x00, KEYSYM_DOWN},
   {0x4d, 0x00, KEYSYM_RIGHT},
   {0x4b, 0x00, KEYSYM_LEFT},
   {0x47, 0x00, KEYSYM_HOME},
   {0x4f, 0x00, KEYSYM_END},
   {0x52, 0x00, KEYSYM_INSERT},
   {0x53, 0x7f, KEYSYM_ASCII},      /* [DELETE] */
   {0x49, 0x00, KEYSYM_PAGE_UP},
   {0x51, 0x00, KEYSYM_PAGE_DOWN},
   {0x3b, 0x00, KEYSYM_F1},
   {0x3c, 0x00, KEYSYM_F2},
   {0x3d, 0x00, KEYSYM_F3},
   {0x3e, 0x00, KEYSYM_F4},
   {0x3f, 0x00, KEYSYM_F5},
   {0x40, 0x00, KEYSYM_F6},
   {0x41, 0x00, KEYSYM_F7},
   {0x42, 0x00, KEYSYM_F8},
   {0x43, 0x00, KEYSYM_F9},
   {0x44, 0x00, KEYSYM_F10},
   {0x85, 0x00, KEYSYM_F11},
   {0x86, 0x00, KEYSYM_F12},
};

/*-- get_key_info --------------------------------------------------------------
 *
 *      Convert a COM32 scancode into a generic key info structure.
 *
 * Parameters
 *      IN comew_scancode: the COM32 scancode
 *      IN key:            pointer to a key inifo structure
 *----------------------------------------------------------------------------*/
static void get_key_info(int com32_scancode, key_code_t *key)
{
   static bool extended;
   size_t i;

   key->sym = KEYSYM_NONE;
   key->ascii = 0;

   if (com32_scancode == KEY_NONE) {
      // nothing to do
   } else if (com32_scancode == 0) {
      extended = true;
   } else if (extended) {
      extended = false;
      for (i = 0; i < ARRAYSIZE(scancodes); i++) {
         if (com32_scancode == scancodes[i].scancode) {
            key->sym = scancodes[i].sym;
            key->ascii = scancodes[i].ascii;
            break;
         }
      }
   } else {
      key->sym = KEYSYM_ASCII;
      key->ascii = com32_scancode;
   }
}

/*-- com32_idle ----------------------------------------------------------------
 *
 *      Wrapper to the 'Idle loop call' COM32 service.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_idle(void)
{
   com32sys_t iregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x13;

   return intcall_check_CF(COM32_INT, &iregs, NULL);
}

/*-- com32_kbd_poll ------------------------------------------------------------
 *
 *      Wrapper to the 'Check Keyboard' COM32 service.
 *
 * Results
 *      True if a keystroke is waiting, false otherwhise.
 *----------------------------------------------------------------------------*/
static bool com32_kbd_poll(void)
{
   com32sys_t iregs, oregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.b[1] = 0x0B;
   intcall(COM32_INT_DOS_COMPATIBLE, &iregs, &oregs);

   return (oregs.eax.b[0] == 0xff);
}

/*-- com32_kbd_read ------------------------------------------------------------
 *
 *      Wrapper to the 'Get Key without Echo' COM32 service.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static uint8_t com32_kbd_read(void)
{
   com32sys_t iregs, oregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.b[1] = 0x08;
   intcall(COM32_INT_DOS_COMPATIBLE, &iregs, &oregs);

   return oregs.eax.b[0];
}

/*-- kbd_waitkey ---------------------------------------------------------------
 *
 *      Get the next keystroke if it represents a valid KEYSYM (defined in
 *      boot_services.h) or an ASCII character. All other key sequences are
 *      ignored. This function blocks until a key is pressed.
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
   int keycode;

   while (!com32_kbd_poll()) {
      com32_idle();
   }

   keycode = com32_kbd_read();
   get_key_info(keycode, key);

   return ERR_SUCCESS;
}

/*-- kbd_waitkey_timeout -------------------------------------------------------
 *
 *      Get the next keystroke if it represents a valid KEYSYM (defined in
 *      boot_services.h) or an ASCII character. All other key sequences are
 *      ignored. This function blocks until a key is pressed or the timeout
 *      expires.
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
   uint32_t start;
   int keycode;
   bool timeout;

   if (nsec == 0) {
      keycode = com32_kbd_poll() ? com32_kbd_read() : KEY_NONE;
   } else {
      start = bios_get_current_tick();
      timeout = false;

      while (!com32_kbd_poll()) {
         if (bios_get_current_tick() - start >= SECONDS_TO_BIOS_TICKS(nsec)) {
            timeout = true;
            break;
         }

         com32_idle();
      }

      keycode = timeout ? KEY_NONE : com32_kbd_read();
   }

   get_key_info(keycode, key);

   return ERR_SUCCESS;
}

/*-- kbd_init ------------------------------------------------------------------
 *
 *      This function flushes stdin and clears the keyboard buffer.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int kbd_init(void)
{
   bios_data_area_t * const bda = bios_get_bda();

   while (com32_kbd_poll()) {
      com32_kbd_read();
   }

   CLI();

   bda->kbd_alt_keypad = 0;
   bda->kbd_head = ptr_real_offset(bda->kbd_buffer, BDA_SEGMENT);
   bda->kbd_tail = ptr_real_offset(bda->kbd_buffer, BDA_SEGMENT);
   memset((void *)bda->kbd_buffer, 0, sizeof (bda->kbd_buffer));

   STI();

   return ERR_SUCCESS;
}
