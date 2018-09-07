/*******************************************************************************
 * Copyright (c) 2008-2012 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * crc.h -- CRC definitions
 */

#ifndef CRC_H_
#define CRC_H_

#include <sys/types.h>
#include <compat.h>

EXTERN uint32_t crc_32(void *buffer, size_t buflen);

#endif /* !CRC_H_ */
