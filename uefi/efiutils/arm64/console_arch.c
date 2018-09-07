/*******************************************************************************
 * Copyright (c) 2008-2018 VMware, Inc.  All rights reserved.
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

/*-- get_serial_port -----------------------------------------------------------
 *
 *      Get the I/O base address of a serial port. On ARM UEFI platforms, we
 *      have to assume a port described by ACPI SPCR.
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
int get_serial_port(UNUSED_PARAM(int com), serial_type_t *type,
                    io_channel_t *io, uint32_t *original_baudrate)
{
   if (acpi_spcr == NULL) {
      return ERR_NOT_FOUND;
   }

   if (acpi_spcr->BaseAddress.AddressSpaceId != EFI_ACPI_5_0_SYSTEM_MEMORY) {
      return ERR_INVALID_PARAMETER;
   }
   io->type = IO_MEMORY_MAPPED;

   io->channel.addr = acpi_spcr->BaseAddress.Address;
   switch (acpi_spcr->InterfaceType) {
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
      io->offset_scaling = acpi_spcr->BaseAddress.RegisterBitWidth / 8;
      break;
   case SPCR_TYPE_PL011:
   case SPCR_TYPE_SBSA_32BIT:
   case SPCR_TYPE_SBSA:
      *type = SERIAL_PL011;
      /*
       * Regs are 32-bit wide, and are likely on a 32-bit boundary.
       */
      io->offset_scaling = acpi_spcr->BaseAddress.RegisterBitWidth / 32;
      if (io->offset_scaling == 0) {
         io->offset_scaling = 1;
      }
      break;
   default:
      return ERR_UNSUPPORTED;
   }

   switch (acpi_spcr->BaudRate) {
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
      return ERR_UNSUPPORTED;
   }

   return ERR_SUCCESS;
}
