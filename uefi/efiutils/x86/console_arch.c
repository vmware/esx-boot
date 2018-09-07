/*******************************************************************************
 * Copyright (c) 2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * console_arch.c -- EFI console management, architecture specific portions.
 */

#include <stdio.h>
#include <stdarg.h>
#include <io.h>

#include "efi_private.h"

#define DEFAULT_COM1         0x3f8
#define DEFAULT_COM2         0x2f8
#define DEFAULT_COM3         0x3e8
#define DEFAULT_COM4         0x2e8

/*-- get_serial_port -----------------------------------------------------------
 *
 *      Get the I/O base address of a COM serial port. On UEFI platforms, we
 *      assume that COM1..4 ports have fixed I/O base addresses.
 *
 * Parameters
 *      IN  com:  1=COM1, 2=COM2, 3=COM3, 4=COM4,
 *                other values: the serial port I/O base address
 *      OUT type: serial port type
 *      OUT io:   io_channel_t describing CSR access
 *      OUT original_baudrate: current baudrate, if known, or
 *                             SERIAL_BAUDRATE_UNKNOWN
 *
 * Results
 *      A generic error status.
 *----------------------------------------------------------------------------*/
int get_serial_port(int com, serial_type_t *type,
                    io_channel_t *io, uint32_t *original_baudrate)
{
   const uint16_t default_com_port[4] = { DEFAULT_COM1, DEFAULT_COM2,
                                          DEFAULT_COM3, DEFAULT_COM4 };

   *type = SERIAL_NS16550;
   io->type = IO_PORT_MAPPED;
   if (com >= 1 && com <= 4) {
      io->channel.port = default_com_port[com - 1];
   } else {
      io->channel.port = com;
   }
   io->offset_scaling = 1;

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

   return error_efi_to_generic(EFI_SUCCESS);
}
