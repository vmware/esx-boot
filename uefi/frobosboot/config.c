/*******************************************************************************
 * Copyright (c) 2015-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * config.c --
 *
 *       Select boot config file according to nvram variable and user input.
 */

#include "frobosboot.h"

#define VAR_NAME L"NextBoot"

#define EFI_VARIABLE_GUID \
   {\
      0xE6FAC0A2, 0x8EC7, 0x4392,                      \
      {0x99, 0x9C, 0x41, 0xAA, 0x5E, 0x87, 0xE2, 0xC6} \
   }

#define VAR_ATTR  EFI_VARIABLE_NON_VOLATILE |        \
                  EFI_VARIABLE_BOOTSERVICE_ACCESS |  \
                  EFI_VARIABLE_RUNTIME_ACCESS

uint32_t bootoption;
uint32_t num_test;
char **test_list = NULL;

/*-- fetch_increment_variable --------------------------------------------------
 *
 *      Fetch the value of the non-volatile variable and increment by 1, if not
 *      found, create one.
 *
 * Parameters
 *      OUT value: buffer to return the contents of the variable
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int fetch_increment_variable(uint32_t *value)
{
   EFI_STATUS Status;
   EFI_GUID varGuid = EFI_VARIABLE_GUID;
   CHAR16 *varName = (CHAR16 *)VAR_NAME;
   UINTN varSize = sizeof(UINTN);
   UINTN tmp;

   Status = rs->GetVariable(varName, &varGuid, NULL, &varSize, value);
   if (Status == EFI_NOT_FOUND) {
      tmp = 0;
      Status = rs->SetVariable(varName, &varGuid, VAR_ATTR, varSize, &tmp);
      *value = 1;
      return error_efi_to_generic(Status);
   } else if (Status == EFI_SUCCESS) {
      tmp = *value + 1;
      Status = rs->SetVariable(varName, &varGuid, VAR_ATTR, varSize, &tmp);
   }

   return error_efi_to_generic(Status);
}

/*-- set_bootoption ------------------------------------------------------------
 *
 *      Set the variable with given boot option.
 *
 * Parameters
 *      IN value: value to be stored in the non-volatile variable
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int set_bootoption(uint32_t value)
{
   EFI_STATUS Status;
   EFI_GUID varGuid = EFI_VARIABLE_GUID;
   CHAR16 *varName = (CHAR16 *)VAR_NAME;

   Status = rs->SetVariable(varName, &varGuid, VAR_ATTR, sizeof(value), &value);

   return error_efi_to_generic(Status);
}

/*-- read_next_bootoption ------------------------------------------------------
 *
 *      Create or increment the variable by 1.
 *
 * Parameters
 *      OUT value: buffer to return the contents of the variable
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int read_next_bootoption(uint32_t *value)
{
   int status;
   status = fetch_increment_variable(value);
   return status;
}

/*-- read_testlist -------------------------------------------------------------
 *
 *      Read testlist from file.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int read_testlist(void)
{
   int status, i;
   void *buffer;
   size_t buflen, len;
   char *p, *newline, *line;
   const char *filename = "/EFI/CONFIG/testlist";

   status = firmware_file_read(filename, NULL, &buffer, &buflen);
   if (status != ERR_SUCCESS) {
      return status;
   }
   p = buffer;
   num_test = 0;
   i = 0;
   while (buflen > 0) {
      newline = memchr(p, '\n', buflen);
      len = (newline != NULL) ? (size_t)(newline - p) : buflen;
      if (len > 0) {
         line = malloc(len + 1);
         if (line == NULL) {
            free(buffer);
            return ERR_OUT_OF_RESOURCES;
         }
         memcpy(line, p, len);
         line[len] = '\0';
         if (num_test == 0) {
            num_test = atoi(line);
            test_list = (char **)malloc(num_test * sizeof (char *));
         } else {
            test_list[i] = strdup(line);
            i++;
         }
         free(line);
         if (newline != NULL) {
           len++;
         }
      }
      buflen -= len;
      p += len;
   }

   return status;
}

/*-- frobos_get_next_boot ------------------------------------------------------
 *
 *      Find the boot config file from the image with multiple frobos tests.
 *
 *      Read the non-volatile counter and decide which kernel to boot, on the
 *      first boot of the image, record the total number of tests in the
 *      counter and decrement the counter during each boot.
 *
 * Parameters
 *      OUT bootconfig: the config file for this boot
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
int frobos_get_next_boot(char *bootconfig)
{
   int status;

   status = read_testlist();
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = read_next_bootoption(&bootoption);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (bootoption >= num_test) {
      bootoption = 0;
   }

   status = setup_display();

   if (status != ERR_SUCCESS) {
      return status;
   }

   bootoption = wait_for_bootoption();

   if (bootoption >= num_test) {
      bootoption = 0;
   }

   set_bootoption(bootoption + 1);

   /* Make the boot config file for this boot. */
   snprintf(bootconfig, BOOT_CFG_LEN, "/EFI/BOOT/%s.cfg",
            test_list[bootoption]);

   return status;
}
