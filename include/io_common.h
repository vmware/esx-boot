/*******************************************************************************
 * Copyright (c) 2008-2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * io_common.h -- common I/O definitions
 */

#ifndef IO_COMMON_H_
#define IO_COMMON_H_

enum {
   IO_PORT_MAPPED,
   IO_MEMORY_MAPPED
};

typedef struct {
   int type;
   union {
      uint16_t port;
      uintptr_t addr;
   } channel;
   uint8_t offset_scaling;
} io_channel_t;

#endif /* !IO_COMMON_H_ */
