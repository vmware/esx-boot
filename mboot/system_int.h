/*******************************************************************************
 * Copyright (c) 2017-2019 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * system_int.h -- system.c and system_arch.c internal header file.
 */

#ifndef SYSTEM_INT_H_
#define SYSTEM_INT_H_

int system_arch_blacklist_memory(void);
void check_cpu_quirks(void);

#endif /* !SYSTEM_INT_H_ */
