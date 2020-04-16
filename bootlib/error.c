/*******************************************************************************
 * Copyright (c) 2018-2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * error.c -- Generic error strings.
*/

#include <bootlib.h>

#define D(symbol, efi_symbol, string) string,
const char *error_str[] = {
   ERROR_TABLE
};
#undef D
