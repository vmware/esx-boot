/*******************************************************************************
 * Copyright (c) 2008-2012,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * uart.c -- Universal Asynchronous Receiver/Transmitter (UART) support
 */

#include <stdint.h>
#include <io.h>
#include <uart.h>

/*-- uart_putc -----------------------------------------------------------------
 *
 *      Write a character on a serial port.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN c:   character to be written
 *----------------------------------------------------------------------------*/
void uart_putc(const uart_t *dev, char c)
{
  if (dev->putc != NULL) {
     dev->putc(dev, c);
  }
}
