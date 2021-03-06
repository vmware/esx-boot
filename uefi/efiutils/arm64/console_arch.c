/*******************************************************************************
 * Copyright (c) 2008-2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * console_arch.c -- EFI console management, architecture specific portions.
 */

#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <fdt_vmware.h>

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

/*-- get_fdt_serial_port -------------------------------------------------------
 *
 *      Attempt to get serial port configuration via FDT.
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
static int get_fdt_serial_port(UNUSED_PARAM(int com), serial_type_t *type,
                               io_channel_t *io, uint32_t *original_baudrate)
{
   void *fdt;
   int node;
   int status;
   const char *prop;
   char *baud;
   int prop_len;

   status = get_fdt(&fdt);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (fdt_check_header(fdt) != 0) {
      return ERR_UNSUPPORTED;
   }

   node = fdt_path_offset(fdt, "/chosen");
   if (node < 0) {
      return ERR_UNSUPPORTED;
   }

   prop = fdt_getprop(fdt, node, "stdout-path", &prop_len);
   if (prop == NULL || prop_len == 0) {
      return ERR_NOT_FOUND;
   }

   /*
    * stdout-path will look like "serial0:1500000", where the thing
    * after the : is the baud rate.
    */

   baud = strchr(prop, ':');
   if (baud != NULL) {
      prop_len = baud - prop;
      baud++;
   }

   node = fdt_path_offset_namelen(fdt, prop, prop_len);
   if (node < 0) {
      return ERR_NOT_FOUND;
   }

   prop = fdt_getprop(fdt, node, "compatible", &prop_len);
   if (prop == NULL || prop_len == 0) {
      return ERR_NOT_FOUND;
   }

   if (!strcmp(prop, "AAPL,s5l-uart") ||
       !strcmp(prop, "apple,uart")) {
      if (fdt_get_reg(fdt, node, "reg", &io->channel.addr) < 0) {
         return ERR_UNSUPPORTED;
      }
      io->type = IO_MEMORY_MAPPED;
      io->offset_scaling = 1;
      if (baud != NULL) {
         *original_baudrate = strtol(baud, NULL, 0);
      } else {
         *original_baudrate = SERIAL_BAUDRATE_UNKNOWN;
      }
      *type = SERIAL_AAPL_S5L;
      return ERR_SUCCESS;
   }

   return ERR_UNSUPPORTED;
}


/*-- get_spcr_serial_port ------------------------------------------------------
 *
 *      Attempt to get serial port configuration via SPCR.
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
static int get_spcr_serial_port(UNUSED_PARAM(int com), serial_type_t *type,
                                io_channel_t *io, uint32_t *original_baudrate)
{
   EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE *spcr =
      (void *) acpi_find_sdt("SPCR");

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

/*-- get_serial_port -----------------------------------------------------------
 *
 *      Get the I/O base address of a serial port. On ARM UEFI platforms, this
 *      is generally the SPCR. One some platforms, other ways may be preferred.
 *
 * Parameters
 *      IN  com:  unused
 *      OUT type: serial port type
 *      OUT io:   serial port base address
 *      OUT original_baudrate: current baudrate from firmware
 *
 * Results
 *      A generic error status.
 *----------------------------------------------------------------------------*/
int get_serial_port(int com, serial_type_t *type, io_channel_t *io,
                    uint32_t *original_baudrate)
{
   int status;

   status = get_nvidia_rshim_console_port(com, type, io, original_baudrate);
   if (status == ERR_SUCCESS) {
      return status;
   }

   status = get_spcr_serial_port(com, type, io, original_baudrate);
   if (status == ERR_SUCCESS) {
      return status;
   }

   status = get_fdt_serial_port(com, type, io, original_baudrate);
   if (status == ERR_SUCCESS) {
      return status;
   }

   return ERR_NOT_FOUND;
}
