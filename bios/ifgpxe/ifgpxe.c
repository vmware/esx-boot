/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * ifgpxe.c
 *
 * Run one command if the environment is gPXE, and a different command if not.
 *
 * USAGE
 *    default ifgpxe.c32 menu.c32 -- gpxelinux.0
 *
 * The above command will start the menu in a gPXE environment. This allows
 * menu options to take advantage of gPXE features, but it does not require the
 * DHCP server to hand out gpxelinux.0 by default.
 */

#include <stdlib.h>
#include <string.h>
#include <bootlib.h>

#include "com32_private.h"

#define SEPARATOR "--"

int main(int argc, char **argv)
{
   char *str;
   int i;

   for (i = 1; i < argc; i++) {
      if (!strcmp(argv[i], SEPARATOR)) {
         break;
      }
   }

   if (isGPXE()) {
      argv++;
      argc = i - 1;
   } else if (i < argc) {
      argv +=  i + 1;
      argc -=  i + 1;
   } else {
      argc = 0;
   }

   if (argc == 0) {
      com32_run_default();
   } else {
      if (argv_to_str(argc, argv, &str) != ERR_SUCCESS) {
         return -1;
      }
      com32_run_command(str);
      sys_free(str);
   }

   return -1;
}
