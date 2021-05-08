/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_fdt.c -- tests fdt-related functionality
 *
 *   test_fdt [-s]
 *
 *      OPTIONS
 *         -s <name.dtb>  Save the FDT blob to name.dtb.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <bootlib.h>
#include <boot_services.h>
#include <libfdt.h>

/*-- test_fdt_init -------------------------------------------------------------
 *
 *      Parse test_fdt command line options.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *      OUT savepath: pointer to filename for saving FDT, if any
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int test_fdt_init(int argc, char **argv, char **savepath)
{
   int opt;

   if (argc == 0 || argv == NULL || argv[0] == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if (argc > 1) {
      optind = 1;
      do {
         opt = getopt(argc, argv, "s:h");
         switch (opt) {
            case -1:
               break;
            case 's':
               *savepath = optarg;
               break;
            case 'h':
            case '?':
            default:
               Log(LOG_ERR, "Usage: %s [-s name.dtb]", argv[0]);
               return ERR_SYNTAX;
         }
      } while (opt != -1);
   }

   return ERR_SUCCESS;
}

/*-- main ----------------------------------------------------------------------
 *
 *      test_fdt main function.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
   int node;
   void *fdt;
   int status;
   int fdt_error;
   char *savename = NULL;

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = test_fdt_init(argc, argv, &savename);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = get_fdt(&fdt);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "There's no Flattened Device Tree present in the system");
      return status;
   }

   fdt_error = fdt_check_header(fdt);
   if (fdt_error != 0) {
      Log(LOG_ERR, "Bad FDT header: %s", fdt_strerror(fdt_error));
      return ERR_UNSUPPORTED;
   }

   Log(LOG_ERR, "FDT blob is at %p", fdt);
   Log(LOG_ERR, "FDT blob is 0x%x bytes", fdt_totalsize(fdt));

   node = fdt_path_offset(fdt, "/");
   if (node >= 0) {
      Log(LOG_ERR, "Running on a '%s', '%s'",
          (char *) fdt_getprop(fdt, node, "compatible", NULL),
          (char *) fdt_getprop(fdt, node, "model", NULL));
   }

   if (savename != NULL) {
      Log(LOG_INFO, "Saving FDT to %s", savename);
      status = firmware_file_write(savename, NULL, fdt, fdt_totalsize(fdt));
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Couldn't save the FDT blob: %s", error_str[status]);
      }
   }

   return ERR_SUCCESS;
}
