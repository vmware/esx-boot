/*******************************************************************************
 * Copyright (c) 2008-2012,2014-2015 VMware, Inc.  All rights reserved.
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
      /*
       * At some point we'll add the ability to reconfigure the baudrate
       * and to patch SPCR.
       */
      if (baudrate != original_baudrate) {
         baudrate = original_baudrate;
         Log(LOG_WARNING, "Cannot override baud rate on ARM yet: using %u\n",
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
