/*******************************************************************************
 * Copyright (c) 2008-2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * console_arch.c -- EFI console management, architecture specific portions.
 */

#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <fdt_vmware.h>
#include "efi_private.h"

/*
 * Microsoft Debug Port Table 2
 * https://docs.microsoft.com/en-us/windows-hardware/drivers/bringup/acpi-debug-port-table
 */
#define SPCR_TYPE_PL011           0x3
#define SPCR_TYPE_MSM8x60         0x4
#define SPCR_TYPE_NVIDIA_16550    0x5
#define SPCR_TYPE_TI_OMAP         0x6
#define SPCR_TYPE_APM88xxxx       0x8
#define SPCR_TYPE_MSM8974         0x9
#define SPCR_TYPE_SAM5250         0xa
#define SPCR_TYPE_IMX6            0xc
#define SPCR_TYPE_SBSA_32BIT      0xd
#define SPCR_TYPE_SBSA            0xe
#define SPCR_TYPE_ARM_DCC         0xf
#define SPCR_TYPE_BCM2835         0x10
#define SPCR_TYPE_SDM845_18432    0x11
#define SPCR_TYPE_16550_HONOR_GAS 0x12
#define SPCR_TYPE_SDM845_7372     0x13

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
   io->access = IO_ACCESS_64;
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
 *      OUT original_baudrate: current baudrate from FDT
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
   const char *baud = NULL;
   static fdt_serial_id_t const match_ids[] = {
      { "AAPL,s5l-uart", SERIAL_AAPL_S5L },
      { "apple,s5l-uart", SERIAL_AAPL_S5L },
      { "apple,uart", SERIAL_AAPL_S5L },
      { NULL, 0 },
   };

   status = get_fdt(&fdt);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (fdt_check_header(fdt) != 0) {
      return ERR_UNSUPPORTED;
   }

   if (fdt_match_serial_port(fdt, "/chosen", "stdout-path", match_ids,
                             &node, type, &baud) != 0) {
     /*
      * Best effort at this point. The Asahi/OpenBSD U-Boot on M1 Macs under some
      * circumstances sets stdout-path to the framebuffer.
      */
      if (fdt_match_serial_port(fdt, "/aliases", "serial0", match_ids,
                                &node, type, &baud) != 0) {
         return ERR_NOT_FOUND;
      }
   }


   if (*type == SERIAL_AAPL_S5L) {
      if (fdt_get_reg(fdt, node, "reg", &io->channel.addr) < 0) {
         return ERR_UNSUPPORTED;
      }
      io->type = IO_MEMORY_MAPPED;
      io->offset_scaling = 1;
      io->access = IO_ACCESS_32;
      if (baud != NULL) {
         *original_baudrate = strtol(baud, NULL, 0);
      } else {
         *original_baudrate = SERIAL_BAUDRATE_UNKNOWN;
      }
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
   io->access = spcr->BaseAddress.AccessSize;

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
   case SPCR_TYPE_16550_HONOR_GAS:
   case SPCR_TYPE_NVIDIA_16550:
      *type = SERIAL_NS16550;
      /*
       * Regs are 8-bit wide, but are likely on a 32-bit boundary.
       */
      io->offset_scaling = spcr->BaseAddress.RegisterBitWidth / 8;
      if (io->access == IO_ACCESS_LEGACY) {
         io->access = IO_ACCESS_8;
      }

      if (io->access != IO_ACCESS_8 &&
          io->access != IO_ACCESS_32) {
         return ERR_UNSUPPORTED;
      }
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
      if (io->access == IO_ACCESS_LEGACY) {
         io->access = IO_ACCESS_32;
      }

      if (io->access != IO_ACCESS_32) {
         return ERR_UNSUPPORTED;
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
int get_serial_port(int com, serial_type_t *type , io_channel_t *io,
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
