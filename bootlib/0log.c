/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * 0log.c -- No-op logging support
 *
 *      If an app or driver does not call log_init(), it will get this
 *      no-op implementation of Log() and the code in log.c will not
 *      be linked in.
 */

#include <bootlib.h>

static void _Log(int level __attribute__ ((__unused__)),
                 const char *fmt __attribute__ ((__unused__)),
                 ...)
{
}

void Log(int level __attribute__ ((__unused__)),
          const char *fmt __attribute__ ((__unused__)),
          ...)
   __attribute__ ((weak, alias ("_Log")));
