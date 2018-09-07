/*******************************************************************************
 * Copyright (c) 2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * error.c -- Generic error strings.
*/

#include <bootlib.h>

#define D(symbol, string) string,
const char *error_str[] = {
   ERROR_TABLE
};
#undef D
