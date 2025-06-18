/*******************************************************************************
 * Copyright (c) 2019-2020 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * acpi.h -- ACPI structures definitions
 */

#ifndef ACPI_H_
#define ACPI_H_

#include <acpi_common.h>

#pragma pack(1)
typedef struct {
   acpi_sdt sdt_header;
   uint64_t base;
   uint64_t size;
#define TMFIFO_CON_OVERRIDES_SPCR_FOR_EARLY_CONSOLE 0x1
#define TMFIFO_CON_OVERRIDES_DBG2                   0x2
#define TMFIFO_NET_OVERRIDES_DBG2                   0x4
   uint64_t flags;
} acpi_nvidia_tmff;
#pragma pack()

#endif /* !ACPI_H_ */
