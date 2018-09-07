/*******************************************************************************
 * Copyright (c) 2008-2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * io.h -- I/O's specific definitions
 */

#ifndef IO_H_
#define IO_H_

#include <compat.h>
#include <io_common.h>

static INLINE uint8_t io_read8(const io_channel_t *ioch, off_t offset)
{
   if (ioch->type == IO_PORT_MAPPED) {
      uint8_t val;
      __asm__ __volatile__ ("inb %w1, %b0"
                            : "=a" (val)
                            : "Nd" (ioch->channel.port + (offset * ioch->offset_scaling)));
      return val;
   }

   /* channel->type == IO_MEMORY_MAPPED */
   return *((volatile uint8_t *) (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

#define io_read16 _Static_assert(0, "io_read16 not implemented")
#define io_read32 _Static_assert(0, "io_read32 not implemented")
#define io_read64 _Static_assert(0, "io_read64 not implemented")

static INLINE void io_write8(const io_channel_t *ioch, off_t offset, uint8_t val)
{
   if (ioch->type == IO_PORT_MAPPED) {
      __asm__ __volatile__ ("outb %b1, %w0"
                            :
                            : "Nd" (ioch->channel.port + (offset * ioch->offset_scaling)),
                              "a" (val));

      /* Magic io delay. */
      __asm__ __volatile__ ("outb %al, $0x80");
   } else {
      /* channel->type == IO_MEMORY_MAPPED */
     *((volatile uint8_t *) (ioch->channel.addr + (offset * ioch->offset_scaling))) = val;
   }
}

#define io_write16 _Static_assert(0, "io_write16 not implemented")
#define io_write32 _Static_assert(0, "io_write32 not implemented")
#define io_write64 _Static_assert(0, "io_write64 not implemented")

#endif /* !IO_H_ */
