/*******************************************************************************
 * Copyright (c) 2008-2012,2014-2015,2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * console.c -- Console management
 */

#include <string.h>
#include <bios.h>
#include <io.h>

#include "com32_private.h"

/*-- set_graphic_mode ----------------------------------------------------------
 *
 *      Set the display to VBE graphic mode. This function does nothing and
 *      returns successfully.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int set_graphic_mode(void)
{
   return ERR_SUCCESS;
}

/*-- com32_putc ----------------------------------------------------------------
 *
 *      Wrapper for the 'Write Character' COM32 service.
 *
 * Parameters
 *      IN c: the character to be printed.
 *----------------------------------------------------------------------------*/
static void com32_putc(uint8_t c)
{
   com32sys_t iregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.b[1] = 0x02;
   iregs.edx.b[0] = c;
   intcall(COM32_INT_DOS_COMPATIBLE, &iregs, NULL);
}

/*-- firmware_print ------------------------------------------------------------
 *
 *      Print a string to the COM32 console.
 *
 * Parameters
 *      IN str: pointer to the ASCII string to print
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_print(const char *str)
{
   for ( ; *str != '\0'; str++) {
      if (*str == '\n') {
         com32_putc('\r');
      }
      com32_putc(*str);
   }

   return ERR_SUCCESS;
}

/*-- get_serial_port -----------------------------------------------------------
 *
 *      Get the I/O base address of a COM serial port.
 *
 * Parameters
 *      IN  com:  1=COM1, 2=COM2, 3=COM3, 4=COM4
 *                other values: the serial port I/O base address
 *      OUT type: serial port type
 *      OUT io:   io_channel_t describing CSR access
 *      OUT original_baudrate: current baudrate, if known, or
 *                             SERIAL_BAUDRATE_UNKNOWN
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_serial_port(int com, serial_type_t *type,
                    io_channel_t *io, uint32_t *original_baudrate)
{
   uint16_t p;

   if (com >= 1 && com <= 4) {
      p = bios_get_com_port(com);
      if (p == 0) {
         return ERR_UNSUPPORTED;
      }
   } else {
      p = com;
   }

   *type = SERIAL_NS16550;

   /*
    * It is always ok to return SERIAL_BAUDRATE_UNKNOWN.
    * The firmware-configured baud rate is only checked
    * in a non-x86 path where we log a warning if the user
    * tries overriding the baud rate through a command-line
    * parameter.
    *
    * Eventually original_baudrate will disappear, as it
    * is a temporary measure.
    */
   *original_baudrate = SERIAL_BAUDRATE_UNKNOWN;
   io->type = IO_PORT_MAPPED;
   io->channel.port = p;
   io->offset_scaling = 1;

   return ERR_SUCCESS;
}
