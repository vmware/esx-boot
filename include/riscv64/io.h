/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
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

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("lb %0, 0(%1)" : "=r" (val) :
                         "r" (ioch->channel.addr + offset * ioch->offset_scaling));
   fence_io_read();

   return val;
}

static INLINE uint16_t io_read16(const io_channel_t *ioch, off_t offset)
{
   uint16_t val;

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("lh %0, 0(%1)" : "=r" (val) :
                         "r" (ioch->channel.addr + offset * ioch->offset_scaling));
   fence_io_read();

   return val;
}

static INLINE uint32_t io_read32(const io_channel_t *ioch, off_t offset)
{
   uint32_t val;

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("lw %0, 0(%1)" : "=r" (val) :
                         "r" (ioch->channel.addr + offset * ioch->offset_scaling));
   fence_io_read();

   return val;
}

static INLINE uint64_t io_read64(const io_channel_t *ioch, off_t offset)
{
   uint64_t val;

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__ ("ld %0, 0(%1)" : "=r" (val) :
                         "r" (ioch->channel.addr + offset * ioch->offset_scaling));
   fence_io_read();

   return val;
}

static INLINE void io_write8(const io_channel_t *ioch, off_t offset, uint8_t val)
{
   fence_io_write();

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("sb %0, 0(%1)" : :
                        "r" (val), "r" (ioch->channel.addr + offset * ioch->offset_scaling));
}

static INLINE void io_write16(const io_channel_t *ioch, off_t offset, uint16_t val)
{
   fence_io_write();

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("sh %0, 0(%1)" : :
                        "r" (val), "r" (ioch->channel.addr + offset * ioch->offset_scaling));
}

static INLINE void io_write32(const io_channel_t *ioch, off_t offset, uint32_t val)
{
   fence_io_write();

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("sw %0, 0(%1)" : :
                        "r" (val), "r" (ioch->channel.addr + offset * ioch->offset_scaling));
}

static INLINE void io_write64(const io_channel_t *ioch, off_t offset, uint64_t val)
{
   fence_io_write();

   /* ioch->type == IO_MEMORY_MAPPED */
   __asm__ __volatile__("sd %0, 0(%1)" : :
                        "r" (val), "r" (ioch->channel.addr + offset * ioch->offset_scaling));
}

#endif /* !IO_H_ */
