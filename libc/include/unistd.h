/*******************************************************************************
 * Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * unistd.h
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <compiler.h>
#include <stddef.h>

EXTERN char *optarg;
EXTERN int optind;
EXTERN int optopt;

EXTERN int getopt(int argc, char *const *argv, const char *optstring);

#endif /* !_UNISTD_H */
