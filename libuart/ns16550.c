/*******************************************************************************
 * Copyright (c) 2008-2015,2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * ns16550.c -- NS 16550-type UART support.
 */

#include <stdint.h>
#include <io.h>
#include <uart.h>
#include "ns16550.h"

/*-- ns16550_read --------------------------------------------------------------
 *
 *      Read a UART register.
 *
 * Parameters
 *      IN dev: Pointer to a UART descriptor
 *      IN reg: register to read from
 *
 * Results:
 *      8-bit register content.
 *----------------------------------------------------------------------------*/
static uint8_t ns16550_read(const uart_t *dev, uint16_t reg)
{
   return io_read8(&dev->io, reg);
}

/*-- ns16550_write -------------------------------------------------------------
 *
 *      Write a UART register.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN reg: register to write to
 *      IN val: value to be written to the given register
 *----------------------------------------------------------------------------*/
static void ns16550_write(const uart_t *dev, uint16_t reg, uint8_t val)
{
   io_write8(&dev->io, reg, val);
}

/*-- ns16550_putc --------------------------------------------------------------
 *
 *      Write a character on a serial port.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN c:   character to be written
 *----------------------------------------------------------------------------*/
static void ns16550_putc(const uart_t *dev, char c)
{
   uint16_t timeout;

   for (timeout = 0xffff; timeout > 0; timeout--) {
      if ((ns16550_read(dev, NS16550_LSR) & NS16550_LSR_THRE) != 0) {
         ns16550_write(dev, NS16550_TX, c);
         return;
      }
   }
}

/*-- ns16550_init --------------------------------------------------------------
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
int ns16550_init(uart_t *dev)
{
   uint8_t c;

   if (dev->type != SERIAL_NS16550) {
      return ERR_UNSUPPORTED;
   }

   /* Word length = 8, no parity, 1 stop bit */
   ns16550_write(dev, NS16550_LCR, NS16550_LCR_WLEN8);
   c = ns16550_read(dev, NS16550_LCR);
   if (c != NS16550_LCR_WLEN8) {
      return ERR_DEVICE_ERROR;
   }

   /* No interrupts */
   ns16550_write(dev, NS16550_IER, 0);

   /*
    * Some null modem cables use a loop-back from the DTR to the DCD line and
    * another from RTS to CTS.
    */
   ns16550_write(dev, NS16550_MCR, NS16550_MCR_OUT2 | NS16550_MCR_RTS | NS16550_MCR_DTR);

#if defined(only_x86)
   {
      uint16_t latch;

      /*
       * Only allow setting baudrates on x86. ESXi-ARM uses the firmware
       * baudrate (e.g. from ACPI SPCR or FDT) and the UART to figure out the
       * UART clock, so unless we patch the firmware data, the OS will get very
       * confused.
       *
       * So today the behavior is to maintain whatever the firmware UART
       * configuration is.
       */
      if (dev->baudrate == 0) {
         return ERR_INVALID_PARAMETER;
      }

      /*
       * Set baudrate. The dividend here matches an x86 computer only, and this
       * will need to probe the correct value when this path is enabled for ARM.
       */
      latch = MAX(115200 / dev->baudrate, 1);
      ns16550_write(dev, NS16550_LCR, c | NS16550_LCR_DLAB);
      ns16550_write(dev, NS16550_DLL, latch & 0xff);
      ns16550_write(dev, NS16550_DLM, latch >> 8);
      ns16550_write(dev, NS16550_LCR, c & ~NS16550_LCR_DLAB);
   }
#endif

   /* Try to enable the FIFO (trigger on 1 bytes) */
   ns16550_write(dev, NS16550_FCR, NS16550_FCR_ENABLE_FIFO);
   c = ns16550_read(dev, NS16550_FCR);
   if (c == NS16550_FCR_ENABLE_FIFO) {
      c |= NS16550_FCR_CLEAR_RCVR | NS16550_FCR_CLEAR_XMIT | NS16550_FCR_TRIGGER_1;
   } else {
      c = 0;
   }
   ns16550_write(dev, NS16550_FCR, c);

   /* Read the line status register to clear the error flags */
   c = ns16550_read(dev, NS16550_LSR);

   dev->putc = ns16550_putc;
   return ERR_SUCCESS;
}
