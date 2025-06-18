/*******************************************************************************
 * Copyright (c) 2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * aapl-s5l.c -- Support for the UART found in Apple Silicon hardware.
 */

#include <stdint.h>
#include <io.h>
#include <uart.h>
#include "aapl-s5l.h"

/*-- aapl_s5l_read --------------------------------------------------------------
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
uint32_t aapl_s5l_read(const uart_t *dev, uint32_t reg)
{
   return io_read32(&dev->io, reg);
}

/*-- aapl_s5l_write -------------------------------------------------------------
 *
 *      Write a UART register.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN reg: register to write to
 *      IN val: value to be written to the given register
 *----------------------------------------------------------------------------*/
static void aapl_s5l_write(const uart_t *dev, uint32_t reg, uint32_t val)
{
   io_write32(&dev->io, reg, val);
}

/*-- aapl_s5l_putc --------------------------------------------------------------
 *
 *      Write a character on a serial port.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN c:   character to be written
 *----------------------------------------------------------------------------*/
static void aapl_s5l_putc(const uart_t *dev, char c)
{
   uint16_t timeout;

   for (timeout = 0xffff; timeout > 0; timeout--) {
      if ((aapl_s5l_read(dev, AAPL_S5L_UTRSTAT) &
           AAPL_S5L_UTRSTAT_TX_FIFO_EMPTY) != 0) {
         aapl_s5l_write(dev, AAPL_S5L_UTXH, c);
         return;
      }
   }
}

/*-- aapl_s5l_init --------------------------------------------------------------
 *
 *      Prepare an S5L UART.
 *
 *      Note: does nothing today, since the UART must already have been
 *      enabled by firmware / prior boot stages. For example, M1N1 will
 *      configure the baud-rate for 1.5Mbaud.
 *
 * Parameters
 *      IN dev:      pointer to a UART descriptor
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int aapl_s5l_init(uart_t *dev)
{
   if (dev->type != SERIAL_AAPL_S5L) {
      return ERR_UNSUPPORTED;
   }

   dev->putc = aapl_s5l_putc;
   return ERR_SUCCESS;
}
