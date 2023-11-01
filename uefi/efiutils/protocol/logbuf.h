/*******************************************************************************
 * Copyright (c) 2023 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#ifndef __VMW_LOGBUF_PROTOCOL_H__
#define __VMW_LOGBUF_PROTOCOL_H__

#define LOGBUF_CURRENT_API_VERSION 1

#define EFI_LOG_PROTOCOL_GUID { \
   0x2c3414cf, 0x83bb, 0x4f77,  \
      { 0x9b, 0xf2, 0x8c, 0x4b, 0xa4, 0x07, 0x4b, 0x90 } \
}

typedef struct _VMW_LOGBUFFER_PROTOCOL VMW_LOGBUFFER_PROTOCOL;

struct _VMW_LOGBUFFER_PROTOCOL {
   uint32_t ApiVersion;
   const char *ModuleVersion;
   struct syslogbuffer *syslogbuf;
};

#endif
