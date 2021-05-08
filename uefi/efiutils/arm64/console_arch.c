/*******************************************************************************
 * Copyright (c) 2008-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * console_arch.c -- EFI console management, architecture specific portions.
 */

#include <stdio.h>
#include <stdarg.h>
#include <io.h>

/*
 * Microsoft Debug Port Table 2 (http://go.microsoft.com/fwlink/p/?LinkId=234837)
 */
#define SPCR_TYPE_PL011       0x3
#define SPCR_TYPE_SBSA_32BIT  0xd
#define SPCR_TYPE_SBSA        0xe
#define SPCR_TYPE_ARM_DCC     0xf
#define SPCR_TYPE_BCM2835     0x10

#include "efi_private.h"

/*-- get_nvidia_rshim_console_port ---------------------------------------------
 *
 *      BlueField-based platforms support a serial console over the PCIe/USB
 *      RSHIM interface.
 *
 * Parameters
 *      IN  com:  unused
 *      OUT type: serial port type
 *      OUT io:   serial port base address
 *      OUT original_baudrate: current baudrate from SPCR
 *
 * Results
 *      A generic error status.
 *----------------------------------------------------------------------------*/
static int get_nvidia_rshim_console_port(UNUSED_PARAM(int com), serial_type_t *type,
                                         io_channel_t *io, uint32_t *original_baudrate)
{
   acpi_nvidia_tmff *tmff = (void *) acpi_find_sdt("TMFF");

   if (tmff == NULL) {
      return ERR_NOT_FOUND;
   }

   if ((tmff->flags & TMFIFO_CON_OVERRIDES_SPCR_FOR_EARLY_CONSOLE) == 0) {
      return ERR_NOT_FOUND;
   }

   io->type = IO_MEMORY_MAPPED;
   io->channel.addr = tmff->base;
   io->offset_scaling = 1;
   *original_baudrate = SERIAL_BAUDRATE_UNKNOWN;
   *type = SERIAL_TMFIFO;

   return ERR_SUCCESS;
}

/*-- get_serial_port -----------------------------------------------------------
 *
 *      Get the I/O base address of a serial port. On ARM UEFI platforms, this
 *      is generally the SPCR. One some platforms, other ways may be preferred.
 *
 * Parameters
 *      IN  com:  unused
 *      OUT type: serial port type
 *      OUT io:   serial port base address
 *      OUT original_baudrate: current baudrate from SPCR
 *
 * Results
 *      A generic error status.
 *----------------------------------------------------------------------------*/
int get_serial_port(int com, serial_type_t *type, io_channel_t *io,
                    uint32_t *original_baudrate)
{
   int status;
   EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE *spcr =
      (void *) acpi_find_sdt("SPCR");

   status = get_nvidia_rshim_console_port(com, type, io, original_baudrate);
   if (status == ERR_SUCCESS) {
      return status;
   }

   if (spcr == NULL) {
      return ERR_NOT_FOUND;
   }

   if (spcr->BaseAddress.AddressSpaceId != EFI_ACPI_5_0_SYSTEM_MEMORY) {
      return ERR_INVALID_PARAMETER;
   }
   io->type = IO_MEMORY_MAPPED;

   io->channel.addr = spcr->BaseAddress.Address;
   switch (spcr->InterfaceType) {
   case SPCR_TYPE_BCM2835:
     /*
      * Just skip the weird stuff, and we get an almost-16550 like UART.
      */
     io->channel.addr += 0x40;
     /*
      * Fall-through.
      */
   case EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_16550:
   case EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_INTERFACE_TYPE_16450:
      *type = SERIAL_NS16550;
      /*
       * Regs are 8-bit wide, but are likely on a 32-bit boundary.
       */
      io->offset_scaling = spcr->BaseAddress.RegisterBitWidth / 8;
      break;
   case SPCR_TYPE_PL011:
   case SPCR_TYPE_SBSA_32BIT:
   case SPCR_TYPE_SBSA:
      *type = SERIAL_PL011;
      /*
       * Regs are 32-bit wide, and are likely on a 32-bit boundary.
       */
      io->offset_scaling = spcr->BaseAddress.RegisterBitWidth / 32;
      if (io->offset_scaling == 0) {
         io->offset_scaling = 1;
      }
      break;
   default:
      return ERR_UNSUPPORTED;
   }

   switch (spcr->BaudRate) {
   case EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_115200:
      *original_baudrate = 115200;
      break;
   case EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_57600:
      *original_baudrate = 57600;
      break;
   case EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_19200:
      *original_baudrate = 19200;
      break;
   case EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_BAUD_RATE_9600:
      *original_baudrate = 9600;
      break;
   default:
      *original_baudrate = SERIAL_BAUDRATE_UNKNOWN;
   }

   return ERR_SUCCESS;
}
