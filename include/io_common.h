/*******************************************************************************
 * Copyright (c) 2008-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * io_common.h -- common I/O definitions
 */

#ifndef IO_COMMON_H_
#define IO_COMMON_H_

#define IO_BAD_ACCESS ((uintptr_t) -1)

typedef enum {
   IO_PORT_MAPPED,
   IO_MEMORY_MAPPED
} io_channel_type_t;

typedef enum {
   /*
    * These match ACPI Generic Address Structure (GAS) Access Size.
    */
   IO_ACCESS_LEGACY = 0,
   IO_ACCESS_8,
   IO_ACCESS_16,
   IO_ACCESS_32,
   IO_ACCESS_64
} io_channel_access_t;

typedef struct {
   io_channel_type_t type;
   union {
      uint16_t port;
      uintptr_t addr;
   } channel;
   uint8_t offset_scaling;
   io_channel_access_t access;
} io_channel_t;

static INLINE uint8_t io_read8(const io_channel_t *ioch, off_t offset);
static INLINE uint16_t io_read16(const io_channel_t *ioch, off_t offset);
static INLINE uint32_t io_read32(const io_channel_t *ioch, off_t offset);
static INLINE uint64_t io_read64(const io_channel_t *ioch, off_t offset);

static INLINE void io_write8(const io_channel_t *ioch, off_t offset, uint8_t val);
static INLINE void io_write16(const io_channel_t *ioch, off_t offset, uint16_t val);
static INLINE void io_write32(const io_channel_t *ioch, off_t offset, uint32_t val);
static INLINE void io_write64(const io_channel_t *ioch, off_t offset, uint64_t val);

static INLINE uintptr_t
io_read(const io_channel_t *ioch, off_t offset)
{
   if (arch_is_x86) {
      if (ioch->type == IO_PORT_MAPPED &&
          ioch->access == IO_ACCESS_64) {
         return IO_BAD_ACCESS;
      }
   } else {
      if (ioch->type == IO_PORT_MAPPED) {
         return IO_BAD_ACCESS;
      }
   }

   switch (ioch->access) {
   default:
   case IO_ACCESS_LEGACY:
   case IO_ACCESS_8:
      return io_read8(ioch, offset);
   case IO_ACCESS_16:
      return io_read16(ioch, offset);
   case IO_ACCESS_32:
      return io_read32(ioch, offset);
   case IO_ACCESS_64:
      return io_read64(ioch, offset);
   }
}

static INLINE void
io_write(const io_channel_t *ioch, off_t offset, uintptr_t val)
{
   if (arch_is_x86) {
      if (ioch->type == IO_PORT_MAPPED &&
          ioch->access == IO_ACCESS_64) {
         return;
      }
   } else {
      if (ioch->type == IO_PORT_MAPPED) {
         return;
      }
   }

   switch (ioch->access) {
   default:
   case IO_ACCESS_LEGACY:
   case IO_ACCESS_8:
      io_write8(ioch, offset, (uint8_t) val);
      break;
   case IO_ACCESS_16:
      io_write16(ioch, offset, (uint16_t) val);
      break;
   case IO_ACCESS_32:
      io_write32(ioch, offset, (uint32_t) val);
      break;
   case IO_ACCESS_64:
      io_write64(ioch, offset, val);
      break;
   }
}

#endif /* !IO_COMMON_H_ */
