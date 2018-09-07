/*******************************************************************************
 * Copyright (c) 2014 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * inet.h
 */

#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <stdint.h>

#define AF_INET  2
#define AF_INET6 10

INLINE uint16_t ntohs(uint16_t netshort)
{
   return ((netshort << 8) | (netshort >> 8));
}

int inet_pton(int af, const char *src, void *dst);

#endif /* !_ARPA_INET_H */
