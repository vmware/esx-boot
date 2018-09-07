/*******************************************************************************
 * Copyright (c) 2008-2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * pl011.c -- ARM PL011-type UART support.
 */

#include <stdint.h>
#include <io.h>
#include <uart.h>
#include "pl011.h"

/*-- pl011_read --------------------------------------------------------------
 *
 *      Read a UART register.
 *
 * Parameters
 *      IN dev: Pointer to a UART descriptor
 *      IN reg: register to read from
 *
 * Results:
 *      32-bit register content.
 *----------------------------------------------------------------------------*/
uint32_t pl011_read(const uart_t *dev, uint32_t reg)
{
   return io_read32(&dev->io, reg);
}

/*-- pl011_write -------------------------------------------------------------
 *
 *      Write a UART register.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN reg: register to write to
 *      IN val: value to be written to the given register
 *----------------------------------------------------------------------------*/
static void pl011_write(const uart_t *dev, uint32_t reg, uint32_t val)
{
   io_write32(&dev->io, reg, val);
}

/*-- pl011_putc --------------------------------------------------------------
 *
 *      Write a character on a serial port.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN c:   character to be written
 *----------------------------------------------------------------------------*/
static void pl011_putc(const uart_t *dev, char c)
{
   uint16_t timeout;

   for (timeout = 0xffff; timeout > 0; timeout--) {
      if ((pl011_read(dev, PL011_FR) & PL011_FR_TXFF) == 0) {
         pl011_write(dev, PL011_DR, c);
         return;
      }
   }
}

/*-- pl011_init --------------------------------------------------------------
 *
 *      Initialize a UART device with the given baudrate.
 *        - polling mode (no interrupts)
 *        - Word length = 8 bits
 *        - 1 stop bit
 *        - no parity
 *        - FIFO triggering on 1 byte
 *
 *      Note: does nothing today, since the UART must already have been
 *      enabled by the firmware. UARTs on ARM64 servers don't have a known
 *      fixed divisor and special ACPI actions may be necessary to enable
 *      UARTs that had been disabled.
 *
 *      In the future, given an SPCR-defined UART, we may allow changing
 *      the baud rate.
 *
 * Parameters
 *      IN dev:      pointer to a UART descriptor
 *      IN baudrate: baud rate, in bits per second
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int pl011_init(uart_t *dev)
{
   if (dev->type != SERIAL_PL011) {
      return ERR_UNSUPPORTED;
   }

   dev->putc = pl011_putc;
   return ERR_SUCCESS;
}
