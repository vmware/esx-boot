/*******************************************************************************
 * Copyright (c) 2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * uri.h -- URI related operations
 */

#ifndef URI_H_
#define URI_H_

#include <boot_services.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
   size_t length;
   key_value_t* parameters;
} query_string_parameters_t;

#endif /* URI_H_ */
