/*******************************************************************************
 * Copyright (c) 2008-2015 Broadcom. All Rights Reserved.
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
#include <cpu.h>

static INLINE uint8_t io_read8(const io_channel_t *ioch, off_t offset)
{
   uint8_t val;

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("ldrb %w0, [%1]" : "=r" (val) :
                         "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
   rmb();

   return val;
}

static INLINE uint16_t io_read16(const io_channel_t *ioch, off_t offset)
{
   uint16_t val;

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("ldrh %w0, [%1]" : "=r" (val) :
                         "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
   rmb();

   return val;
}

static INLINE uint32_t io_read32(const io_channel_t *ioch, off_t offset)
{
   uint32_t val;

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("ldr %w0, [%1]" : "=r" (val) :
                         "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
   rmb();

   return val;
}

static INLINE uint64_t io_read64(const io_channel_t *ioch, off_t offset)
{
   uint64_t val;

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("ldr %x0, [%1]" : "=r" (val) :
                         "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
   rmb();

   return val;
}

static INLINE void io_write8(const io_channel_t *ioch, off_t offset, uint8_t val)
{
   wmb();

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("strb %w0, [%1]" : :
                        "r" (val), "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

static INLINE void io_write16(const io_channel_t *ioch, off_t offset, uint16_t val)
{
   wmb();

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("strh %w0, [%1]" : :
                        "r" (val), "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

static INLINE void io_write32(const io_channel_t *ioch, off_t offset, uint32_t val)
{
   wmb();

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("str %w0, [%1]" : :
                        "r" (val), "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

static INLINE void io_write64(const io_channel_t *ioch, off_t offset, uint64_t val)
{
   wmb();

   /* channel->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("str %x0, [%1]" : :
                        "r" (val), "r" (ioch->channel.addr + (offset * ioch->offset_scaling)));
}

#endif /* !IO_H_ */
