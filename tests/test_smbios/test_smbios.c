/*******************************************************************************
 * Copyright (c) 2017 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_smbios.c -- tests smbios-related functionality
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <bootlib.h>
#include <boot_services.h>

/*-- test_smbios_init ----------------------------------------------------------
 *
 *      Parse test_smbios command line options.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int test_smbios_init(int argc, char **argv)
{
   int opt;

   if (argc == 0 || argv == NULL || argv[0] == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if (argc > 1) {
      optind = 1;
      do {
         opt = getopt(argc, argv, "?");
         switch (opt) {
            case -1:
               break;
            case '?':
               Log(LOG_ERR, "No help available (and no options)\n");
               break;
            default:
               return ERR_SYNTAX;
         }
      } while (opt != -1);
   }

   return ERR_SUCCESS;
}

static int test_tables(void *eps_start, size_t eps_length,
                       void *table_start, size_t table_length)
{
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry ptr;

   Log(LOG_INFO, "SMBIOS entry point %zu bytes at %p\n",
       eps_length, eps_start);
   if (!is_valid_firmware_table(eps_start, eps_length) ||
       eps_length < SMBIOS_EPS3_SIGNATURE_LEN) {
      Log(LOG_ERR, "Corrupt SMBIOS entry point");
      return ERR_UNSUPPORTED;
   }

   if (memcmp(eps_start, SMBIOS_EPS_SIGNATURE, SMBIOS_EPS_SIGNATURE_LEN) == 0) {
      Log(LOG_INFO, "32-bit SMBIOS tables\n");
   } else if (memcmp(eps_start, SMBIOS_EPS3_SIGNATURE, SMBIOS_EPS3_SIGNATURE_LEN) == 0) {
      Log(LOG_INFO, "64-bit SMBIOS tables\n");
   } else {
      Log(LOG_ERR, "Unknown kind of SMBIOS tables");
      return ERR_UNSUPPORTED;
   }

   Log(LOG_INFO, "SMBIOS tables %zu bytes at %p\n",
       table_length, table_start);
   smbios_start.raw_bytes = table_start;
   smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;
   if (smbios_get_struct(smbios_start, smbios_end, 1, &ptr) == ERR_SUCCESS) {
      char *s;

      Log(LOG_INFO, "Have a Type 1 structure\n");
      s = smbios_get_string(ptr, smbios_end, ptr.type1->manufacturer);
      if (s != NULL) {
         Log(LOG_INFO, "Manufacturer: %s\n", s);
      }

      s = smbios_get_string(ptr, smbios_end, ptr.type1->product_name);
      if (s != NULL) {
         Log(LOG_INFO, "Product: %s\n", s);
      }

      s = smbios_get_string(ptr, smbios_end, ptr.type1->version);
      if (s != NULL) {
         Log(LOG_INFO, "Version: %s\n", s);
      }

      s = smbios_get_string(ptr, smbios_end, ptr.type1->serial_number);
      if (s != NULL) {
         Log(LOG_INFO, "Serial Number: %s\n", s);
      }

      s = smbios_get_string(ptr, smbios_end, ptr.type1->sku);
      if (s != NULL) {
         Log(LOG_INFO, "SKU: %s\n", s);
      }

      s = smbios_get_string(ptr, smbios_end, ptr.type1->family);
      if (s != NULL) {
         Log(LOG_INFO, "Family: %s\n", s);
      }
   }

   return ERR_SUCCESS;
}

/*-- main ----------------------------------------------------------------------
 *
 *      test_smbios main function.
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
   int status;
   void *eps_start;
   size_t eps_length;
   void *table_start;
   size_t table_length;

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = test_smbios_init(argc, argv);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = smbios_get_info(&eps_start, &eps_length, &table_start, &table_length);
   if (status == ERR_SUCCESS && eps_length != 0) {
      status = test_tables(eps_start, eps_length, table_start, table_length);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Legacy 32-bit SMBIOS test failed\n");
         return status;
      }
   } else {
      Log(LOG_WARNING, "No legacy 32-bit SMBIOS found\n");
      return ERR_SUCCESS;
   }

   status = smbios_get_v3_info(&eps_start, &eps_length, &table_start, &table_length);
   if (status == ERR_SUCCESS && eps_length != 0) {
      status = test_tables(eps_start, eps_length, table_start, table_length);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "v3 64-bit SMBIOS test failed\n");
         return status;
      }
   } else {
      Log(LOG_WARNING, "No v3 64-bit SMBIOS found\n");
      return ERR_SUCCESS;
   }

   return ERR_SUCCESS;
}
