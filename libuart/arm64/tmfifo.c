/*******************************************************************************
 * Copyright (c) 2020 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tmfifo.c -- Virtual console over NVIDIA BlueField RSHIM interface.
 */

#include <stdint.h>
#include <io.h>
#include <uart.h>

#define TMFIFO_MSG_CONSOLE   3

#define TILE_TO_HOST_DATA    0xa40
#define TILE_TO_HOST_STATUS  0xa48
#define TILE_TO_HOST_CTL     0xa50
#define SCRATCHPAD1          0xc20

#define FIFO_LENGTH          256
#define TMFIFO_CHECK_SECONDS 1

typedef union {
  struct {
    uint8_t type;      /* message type. */
    uint8_t len_hi;    /* payload length, high 8 bits. */
    uint8_t len_lo;    /* payload length, low 8 bits. */
    uint8_t unused[5]; /* reserved, set to 0. */
  };
  uint64_t data;
} tmfifo_msg_header_t;

const tmfifo_msg_header_t tx_header = {
  .type = TMFIFO_MSG_CONSOLE,
  .len_lo = 1,
};

/*-- tmfifo_full ---------------------------------------------------------------
 *
 *      Returns whether the TX FIFO is full.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *
 * Results
 *      True if TXFIFO if full. False otherwise.
 *----------------------------------------------------------------------------*/
static bool tmfifo_full(const uart_t *dev)
{
   /*
    * Full means we can't stuff another two packets (message header and the
    * actual data).
    */
   return io_read64(&dev->io, TILE_TO_HOST_STATUS) > (FIFO_LENGTH - 2);
}

/*-- tmfifo_connected ----------------------------------------------------------
 *
 *      Returns whether the remote end (rshim driver) is present and that
 *      the FIFO appears to be draining.
 *
 *      This function attempts to detect connection drop on the remote end
 *      and other issues where the TX FIFO doesn't appear to be draining.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *
 * Results
 *      True if the remote end (rshim driver) is present.
 *----------------------------------------------------------------------------*/
static bool tmfifo_connected(const uart_t *dev)
{
   enum tmfifo_state {
      TMF_NOT_CONNECTED,
      TMF_CONNECTED,
      /*
       * Means that last check the state was connected but FIFO was full. If
       * the FIFO remains full while in this state, we'll report that it
       * is disconnected, but for all intents and purposes treat it
       * as a connected state (probing every TMFIFO_CHECK_SECONDS).
       */
      TMF_CONNECTED_BUT_FULL,
   };
   static enum tmfifo_state last_state = TMF_NOT_CONNECTED;
   static uint64_t last_connected_cnt = 0;

   /*
    * The connection states are checked every TMFIFO_CHECK_SECONDS, unless
    * the state was TMF_NOT_CONNECTED, in which case the check is always
    * done.
    */

   if (last_state == TMF_NOT_CONNECTED ||
       rdtsc() > (last_connected_cnt + TMFIFO_CHECK_SECONDS * tscfreq())) {
      bool connected = io_read64(&dev->io, SCRATCHPAD1) != 0;

      if (connected) {
         /*
          * If we return TMFF as being alive, clear the "alive" magic.
          * If the remote driver doesn't set it again within
          * TMFIFO_CHECK_SECONDS, we'll treat TMFF as dead.
          *
          * This can only happen if the remote driver is unloaded,
          * or if the rshim interface is unplugged, etc.
          */
         io_write64(&dev->io, SCRATCHPAD1, 0);
         last_connected_cnt = rdtsc();
      }

      switch (last_state) {
      case TMF_NOT_CONNECTED:
         if (connected) {
            last_state = TMF_CONNECTED;
         }
         break;
      case TMF_CONNECTED:
      case TMF_CONNECTED_BUT_FULL:
         if (!connected) {
            last_state = TMF_NOT_CONNECTED;
         } else {
            if (tmfifo_full(dev)) {
               last_state = TMF_CONNECTED_BUT_FULL;
            } else {
               last_state = TMF_CONNECTED;
            }
         }
         break;
      }
   }

   return last_state == TMF_CONNECTED;
}

/*-- tmfifo_putc ---------------------------------------------------------------
 *
 *      Write a character to the TMFIFO console.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN c:   character to be written
 *----------------------------------------------------------------------------*/
static void tmfifo_putc(const uart_t *dev, char c)
{
   while (tmfifo_connected(dev)) {
      /*
       * If the FIFO continues being full after TMFIFO_CHECK_SECONDS,
       * tmfifo_connected will time out and return false.
       */
      if (!tmfifo_full(dev)) {
         io_write64(&dev->io, TILE_TO_HOST_DATA, tx_header.data);
         io_write64(&dev->io, TILE_TO_HOST_DATA, c);
         return;
      }
   }
}

/*-- tmfifo_init --------------------------------------------------------------
 *
 *      Prepares a TMFIFO console.
 *
 * Parameters
 *      IN dev:      pointer to a UART descriptor
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int tmfifo_init(uart_t *dev)
{
   if (dev->type != SERIAL_TMFIFO) {
      return ERR_UNSUPPORTED;
   }

   dev->putc = tmfifo_putc;
   dev->flags = UART_USE_AFTER_EXIT_BOOT_SERVICES;
   return ERR_SUCCESS;
}
