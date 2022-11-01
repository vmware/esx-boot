/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_acpi.c -- tests ACPI-related functionality
 *
 *   test_acpi [-iu]
 *
 *      OPTIONS
 *         -i <name.aml>  Install the ACPI table name.aml.
 *         -u <key>       Uninstall the ACPI table identified by key.
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

static int iflag = 0;   /* install ACPI table */
static int uflag = 0;   /* uninstall ACPI table */
static int dflag = 0;   /* dump ACPI table */

/*-- usage ---------------------------------------------------------------------
 *
 *      Log usage message.
 *
 * Parameters
 *      IN progname: program name string
 *----------------------------------------------------------------------------*/
static void usage(char *progname)
{
   Log(LOG_ERR, "Usage: %s [-i name.aml]", progname);
   Log(LOG_ERR, "       %s [-u key]", progname);
   Log(LOG_ERR, "       %s [-d signature]", progname);
}

/*-- test_acpi_init ------------------------------------------------------------
 *
 *      Parse test_acpi command line options.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *      OUT tablepath: pointer to filename for installing ACPI table, if any
 *      OUT tablekey: pointer to key for uninstalling ACPI table, if any
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int test_acpi_init(int argc, char **argv, char **tablepath,
                          unsigned int *tablekey, char **signature)
{
   int opt;

   if (argc == 0 || argv == NULL || argv[0] == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if (argc > 1) {
      optind = 1;
      do {
         opt = getopt(argc, argv, "d:i:hu:");
         switch (opt) {
            case -1:
               break;
            case 'd':
               if (strlen(optarg) != 4) {
                  return ERR_SYNTAX;
               }
               dflag = 1;
               *signature = optarg;
               break;
            case 'i':
               iflag = 1;
               *tablepath = optarg;
               break;
            case 'u':
               if (!is_number(optarg)) {
                  return ERR_SYNTAX;
               }
               uflag = 1;
               *tablekey = (unsigned int)strtoul(optarg, NULL, 10);
               break;
            case 'h':
            case '?':
            default:
               usage(argv[0]);
               return ERR_SYNTAX;
         }
      } while (opt != -1);
   }

   if (dflag + iflag + uflag != 1) {
      usage(argv[0]);
      return ERR_SYNTAX;
   }

   return ERR_SUCCESS;
}

/*-- main ----------------------------------------------------------------------
 *
 *      test_acpi main function.
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
   char *tablepath = NULL, *signature = NULL;
   char oem_id[7], table_id[9];
   unsigned int tablekey = 0;
   acpi_sdt *sdt;
   void *table;
   size_t tablelen;
   int status;

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   acpi_init();

   status = test_acpi_init(argc, argv, &tablepath, &tablekey, &signature);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = acpi_is_present();
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "There's no ACPI support present in the system");
      return status;
   }

   if (dflag) {
      sdt = acpi_find_sdt(signature);
      if (sdt == NULL) {
         Log(LOG_ERR, "Couldn't find ACPI table with sig '%s'", signature);
         return ERR_NOT_FOUND;
      }
      memcpy(oem_id, sdt->oem_id, 6);
      oem_id[6] = '\0';
      memcpy(table_id, sdt->table_id, 8);
      table_id[8] = '\0';
      Log(LOG_INFO, "%s: Length %u, Revision 0x%02x, Checksum 0x%02x",
          signature, sdt->length, sdt->revision, sdt->checksum);
      Log(LOG_INFO, "      OEM ID '%s', Table ID '%s', OEM Revision 0x%08x",
          oem_id, table_id, sdt->oem_revision);
      Log(LOG_INFO, "      Creator ID 0x%08x, Creator Revision 0x%08x",
          sdt->creator_id, sdt->creator_revision);
   } else if (iflag) {
      Log(LOG_INFO, "Installing '%s'", tablepath);
      status = firmware_file_read(tablepath, NULL, &table, &tablelen);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Couldn't read the ACPI table: %s", error_str[status]);
         return status;
      }
      status = acpi_install_table(table, tablelen, &tablekey);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Couldn't install the ACPI table: %s",
             error_str[status]);
         return status;
      }
      Log(LOG_INFO, "Table key: %u", tablekey);
   } else if (uflag) {
      Log(LOG_INFO, "Uninstalling ACPI table with key %u", tablekey);
      status = acpi_uninstall_table(tablekey);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Couldn't uninstall the ACPI table: %s",
             error_str[status]);
         return status;
      }
   }

   return ERR_SUCCESS;
}
