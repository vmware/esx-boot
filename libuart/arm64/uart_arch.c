/*******************************************************************************
 * Copyright (c) 2008-2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * uart_arch.c -- Architecture-specific UART support
 */

#include <stdint.h>
#include <io.h>
#include <uart.h>

int pl011_init(const uart_t *dev);
int ns16550_init(const uart_t *dev);

/*-- uart_init -----------------------------------------------------------------
 *
 *      Initialize a UART device with the given baudrate.
 *        - polling mode (no interrupts)
 *        - Word length = 8 bits
 *        - 1 stop bit
 *        - no parity
 *        - FIFO triggering on 1 byte
 *
 * Parameters
 *      IN dev:      pointer to a UART descriptor
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int uart_init(const uart_t *dev)
{
   int status;

   status = pl011_init(dev);
   if (status != ERR_SUCCESS) {
      /*
       * Some pre-SBSA ARM64 servers may use a 16550/8250-like UART, as may
       * some non-SBSA ARM64 hardware.
       */
      status = ns16550_init(dev);
   }

   return status;
}
