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
   const char *baud = NULL;
   static fdt_serial_id_t const match_ids[] = {
      { "snps,dw-apb-uart", SERIAL_NS16550 },
      { NULL, 0 }
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
      if (fdt_match_serial_port(fdt, "/aliases", "serial0", match_ids,
                                &node, type, &baud) != 0) {
         return ERR_NOT_FOUND;
      }
   }

   if (*type == SERIAL_NS16550) {
      const uint32_t *data;

      if (fdt_get_reg(fdt, node, "reg", &io->channel.addr) < 0) {
         return ERR_UNSUPPORTED;
      }
      io->type = IO_MEMORY_MAPPED;

      data = fdt_getprop(fdt, node, "reg-shift", NULL);
      if (data == NULL) {
         io->offset_scaling = 1;
      } else {
         io->offset_scaling = 1 << fdt32_to_cpu(*data);
      }

      data = fdt_getprop(fdt, node, "reg-io-width", NULL);
      if (data == NULL) {
         io->access = IO_ACCESS_8;
      } else {
         switch (fdt32_to_cpu(*data)) {
         case 4:
            io->access = IO_ACCESS_32;
            break;
         case 1:
            io->access = IO_ACCESS_8;
            break;
         default:
            return ERR_UNSUPPORTED;
         }
      }

      if (baud != NULL) {
         *original_baudrate = strtol(baud, NULL, 0);
      } else {
         *original_baudrate = SERIAL_BAUDRATE_UNKNOWN;
      }
      return ERR_SUCCESS;
   }

   return ERR_UNSUPPORTED;
}


/*-- get_serial_port -----------------------------------------------------------
 *
 *      Get the I/O base address of a serial port.
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
int get_serial_port(UNUSED_PARAM(int com),
                    UNUSED_PARAM(serial_type_t *type),
                    UNUSED_PARAM(io_channel_t *io),
                    UNUSED_PARAM(uint32_t *original_baudrate))
{
   int status;

   status = get_fdt_serial_port(com, type, io, original_baudrate);
   if (status == ERR_SUCCESS) {
      return status;
   }

   return ERR_NOT_FOUND;
}
