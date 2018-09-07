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
   return ns16550_init(dev);
}
