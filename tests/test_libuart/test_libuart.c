/*******************************************************************************
 * Copyright (c) 2015-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_libuart.c -- tests libuart functionality
 *
 *   test_libuart [-sS]
 *
 *      OPTIONS
 *         -S <1...4>     Set the default serial port (1=COM1, 2=COM2, 3=COM3,
 *                        4=COM4, 0xNNNN=hex I/O port address ).
 *         -s <BAUDRATE>  Set the serial port speed to BAUDRATE (in bits per
 *                        second).
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <bootlib.h>
#include <boot_services.h>
#include <uart.h>

static int serial_com;
static int serial_speed;

#define DEFAULT_PROG_NAME       "test_libuart.c32"

static const char *serial_type_names[] = {
#define SERIAL_TYPE_DEF(x) #x,
   SERIAL_TYPE_DEFS
#undef SERIAL_TYPE_DEF
};


/*-- test_libuart_init ----------------------------------------------------------
 *
 *      Parse test_libuart command line options.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int test_libuart_init(int argc, char **argv)
{
   int opt;

   if (argc == 0 || argv == NULL || argv[0] == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   serial_com = DEFAULT_SERIAL_COM;
   serial_speed = DEFAULT_SERIAL_BAUDRATE;

   if (argc > 1) {
      optind = 1;
      do {
         opt = getopt(argc, argv, "s:S:");
         switch (opt) {
            case -1:
               break;
            case 'S':
               serial_com = strtol(optarg, NULL, 0);
               break;
            case 's':
               if (!is_number(optarg)) {
                  return ERR_SYNTAX;
               }
               serial_speed = atoi(optarg);
               break;
            case '?':
            default:
               return ERR_SYNTAX;
         }
      } while (opt != -1);
   }

   return ERR_SUCCESS;
}

/*-- main ----------------------------------------------------------------------
 *
 *      test_libuart main function.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
   int status;
   uart_t serial_dev;
   uint32_t original_baudrate;

   status = test_libuart_init(argc, argv);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   memset(&serial_dev, 0, sizeof (uart_t));
   status = get_serial_port(serial_com, &serial_dev.type,
                            &serial_dev.io, &original_baudrate);
   if (status != ERR_SUCCESS) {
     Log(LOG_ERR, "get_serial_port(%u) failed: %s\n",
         serial_com, error_str[status]);
     return status;
   }

   if (serial_dev.type >= SERIAL_COUNT) {
      Log(LOG_ERR, "corrupted serial_dev.type = %u\n", serial_dev.type);
      return status;
   }

   if (original_baudrate == SERIAL_BAUDRATE_UNKNOWN) {
      Log(LOG_ERR, "port %u is a %s, unknown baud", serial_com,
          serial_type_names[serial_dev.type]);
   } else {
      Log(LOG_ERR, "port %u is a %s, %u baud", serial_com,
          serial_type_names[serial_dev.type],
          original_baudrate);
   }

   Log(LOG_ERR, "registers at %s 0x%"PRIxPTR"\n",
       serial_dev.io.type == IO_PORT_MAPPED ? "io" : "mmio",
       serial_dev.io.channel.addr);
   Log(LOG_ERR, "Register offset scaling = %u\n",
       serial_dev.io.offset_scaling);
   if (serial_dev.io.access == IO_ACCESS_LEGACY) {
      Log(LOG_ERR, "Register access = legacy\n");
   } else {
      Log(LOG_ERR, "Register access = %u bytes\n",
          8 << (serial_dev.io.access - 1));
   }

   serial_dev.id = serial_com;
   if (arch_is_x86) {
      serial_dev.baudrate = serial_speed;
   } else {
      serial_dev.baudrate = original_baudrate;
   }

   status = uart_init(&serial_dev);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "uart_init failed: %s\n", error_str[status]);
      return status;
   }

   status = serial_log_init(serial_com, serial_speed);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "serial_log_init failed: %s\n", error_str[status]);
      return status;
   }

   Log(LOG_ERR, "log via firmare and serial, this should appear twice\n\n\n\n");
   return status;
}
