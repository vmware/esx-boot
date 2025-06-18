/*******************************************************************************
 * Copyright (c) 2008-2020 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * serial.c -- Serial console support
 */

#include <stdint.h>
#include <boot_services.h>
#include <bootlib.h>
#include <uart.h>

static uart_t serial_dev;

/*-- serial_log ----------------------------------------------------------------
 *
 *      Write a string to the serial console.
 *
 * Parameters
 *      IN msg: pointer to the string to be written
 *
 * Results
 *      The number of characters actually written.
 *----------------------------------------------------------------------------*/
static int serial_log(const char *msg)
{
   int len, level;
   bool newline;

   newline = true;
   len = 0;

   if ((uart_flags(&serial_dev) & UART_USE_AFTER_EXIT_BOOT_SERVICES) != 0 &&
       in_boot_services()) {
      /*
       * UART doesn't want to be used until firmware is quiesced (e.g.
       * the same UART is known to be in use by firmware leading to
       * garbaged output).
       */
      return len;
   }

   for ( ; *msg != '\0'; msg++) {
      if (newline && syslog_get_message_level(msg, &level) == ERR_SUCCESS) {
         msg += 3;
         if (*msg == '\0') {
            break;
         }
      }

      if (*msg == '\n') {
         uart_putc(&serial_dev, '\r');
         len++;
         newline = true;
      } else {
         newline = false;
      }

      uart_putc(&serial_dev, *msg);
      len++;
   }

   return len;
}

/*-- serial_log_init -----------------------------------------------------------
 *
 *      Initialize the serial console.
 *
 * Parameters
 *      IN com:      serial port COM number (1=COM1, 2=COM2, 3=COM3, 4=COM4),
 *                   other values: the serial port I/O base address
 *      IN baudrate: serial port speed, in bits per second
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int serial_log_init(int com, uint32_t baudrate)
{
   int status;
   uint32_t original_baudrate;

   if (baudrate == 0 || com < 1 || com > 0xffff) {
      return ERR_INVALID_PARAMETER;
   }

   memset(&serial_dev, 0, sizeof (uart_t));

   status = get_serial_port(com, &serial_dev.type,
                            &serial_dev.io, &original_baudrate);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (arch_is_arm64) {
      if (baudrate != original_baudrate &&
          original_baudrate != SERIAL_BAUDRATE_UNKNOWN) {
         baudrate = original_baudrate;
         Log(LOG_WARNING, "Cannot override baud rate on Arm: using %u\n",
             baudrate);
      }
   }

   serial_dev.id = com;
   serial_dev.baudrate = baudrate;

   status = uart_init(&serial_dev);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = log_subscribe(serial_log, LOG_DEBUG);
   if (status != ERR_SUCCESS) {
      return status;
   }

   return ERR_SUCCESS;
}
