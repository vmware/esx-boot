/*******************************************************************************
 * Copyright (c) 2008-2012,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * uart.h -- Universal Asynchronous Receiver/Transmitter (UART) interface
 */

#ifndef UART_H_
#define UART_H_

#include <stdint.h>
#include <boot_services.h>

typedef struct uart_t {
   int id;
   uint32_t baudrate;
   io_channel_t io;
   void (*putc)(const struct uart_t *dev, char c);
   serial_type_t type;
} uart_t;

int  uart_init(const uart_t *dev);
void uart_putc(const uart_t *dev, char c);

#endif /* !UART_H_ */
