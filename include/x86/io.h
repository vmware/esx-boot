/*******************************************************************************
 * Copyright (c) 2008-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

static INLINE uint16_t io_read16(const io_channel_t *ioch, off_t offset)
{
   if (ioch->type == IO_PORT_MAPPED) {
      uint16_t val;
      __asm__ __volatile__ ("inw %w1, %w0"
                            : "=a" (val)
                            : "Nd" (ioch->channel.port + (offset * ioch->offset_scaling)));
      return val;
   }

   /* channel->type == IO_MEMORY_MAPPED */
   return *((volatile uint16_t *) (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

static INLINE uint32_t io_read32(const io_channel_t *ioch, off_t offset)
{
   if (ioch->type == IO_PORT_MAPPED) {
      uint32_t val;
      __asm__ __volatile__ ("inl %w1, %0"
                            : "=a" (val)
                            : "Nd" (ioch->channel.port + (offset * ioch->offset_scaling)));
      return val;
   }

   /* channel->type == IO_MEMORY_MAPPED */
   return *((volatile uint32_t *) (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

static INLINE uint64_t io_read64(const io_channel_t *ioch, off_t offset)
{
   /* channel->type == IO_MEMORY_MAPPED */
   return *((volatile uint64_t *) (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

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

static INLINE void io_write16(const io_channel_t *ioch, off_t offset, uint16_t val)
{
   if (ioch->type == IO_PORT_MAPPED) {
      __asm__ __volatile__ ("outw %w1, %w0"
                            :
                            : "Nd" (ioch->channel.port + (offset * ioch->offset_scaling)),
                              "a" (val));

      /* Magic io delay. */
      __asm__ __volatile__ ("outb %al, $0x80");
   } else {
      /* channel->type == IO_MEMORY_MAPPED */
      *((volatile uint16_t *) (ioch->channel.addr + (offset * ioch->offset_scaling))) = val;
   }
}

static INLINE void io_write32(const io_channel_t *ioch, off_t offset, uint32_t val)
{
   if (ioch->type == IO_PORT_MAPPED) {
      __asm__ __volatile__ ("outl %1, %w0"
                            :
                            : "Nd" (ioch->channel.port + (offset * ioch->offset_scaling)),
                              "a" (val));

      /* Magic io delay. */
      __asm__ __volatile__ ("outb %al, $0x80");
   } else {
      /* channel->type == IO_MEMORY_MAPPED */
      *((volatile uint32_t *) (ioch->channel.addr + (offset * ioch->offset_scaling))) = val;
   }
}

static INLINE void io_write64(const io_channel_t *ioch, off_t offset, uint64_t val)
{
   /* channel->type == IO_MEMORY_MAPPED */
   *((volatile uint64_t *) (ioch->channel.addr + (offset * ioch->offset_scaling))) = val;
}

#endif /* !IO_H_ */
