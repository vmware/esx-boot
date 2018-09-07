/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
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
