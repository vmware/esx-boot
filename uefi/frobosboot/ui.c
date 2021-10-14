/*******************************************************************************
 * Copyright (c) 2015-2017,2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

 /*
  * ui.c --
  *
  *      User interface for frobos uefi native boot.
  */

#include "frobosboot.h"

#define ASCII_ENTER  0x0d

#define TEXT_MODE    0

#define MARGIN       2

#define EFI_MESSAGE_BUFLEN   128

UINTN columns;
UINTN rows;
UINTN current_row;
UINTN current_page;
UINTN first_row;
UINTN last_row;
UINTN num_pagetest;
UINTN max_page;
char header[200];

/*-- print_current_page --------------------------------------------------------
 *
 *      Print current page of test list.
 *
 *----------------------------------------------------------------------------*/
static void print_current_page(void)
{
   uint32_t i;
   uint32_t pos = current_page * num_pagetest;

   st->ConOut->ClearScreen(st->ConOut);
   st->ConOut->SetCursorPosition(st->ConOut, 0, 0);
   firmware_print(header);
   for (i = 0; i < num_pagetest; i++) {
      if (pos < num_test) {
         firmware_print(test_list[pos]);
         firmware_print("\n");
         pos++;
      } else {
         break;
      }
   }
}

/*-- print_previous_page -------------------------------------------------------
 *
 *      Print previous page of test list.
 *
 *----------------------------------------------------------------------------*/
static void print_previous_page(void)
{
   if (current_page != 0) {
      current_page--;
      print_current_page();
      st->ConOut->SetCursorPosition(st->ConOut, 0, last_row);
      current_row = last_row;
   }
}

/*-- print_next_page -----------------------------------------------------------
 *
 *      Print next page of test list.
 *
 *----------------------------------------------------------------------------*/
static void print_next_page(void)
{
   if (current_page < max_page) {
      current_page++;
      print_current_page();
      st->ConOut->SetCursorPosition(st->ConOut, 0, first_row);
      current_row = first_row;
   }
}

/*-- wait_for_bootoption -------------------------------------------------------
 *
 *      Wait for user input or return default bootoption.
 *
 *      Review and select test by UP/DOWN, PageUp/PageDown, or enter the test
 *      number, press 'Enter' after input to boot.
 *
 * Results
 *      Boot option number(test number).
 *----------------------------------------------------------------------------*/
uint32_t wait_for_bootoption(void)
{
    key_code_t key;
    int wait = 0;
    uint32_t num_inputs = 0;
    uint32_t default_option = bootoption;
    bootoption = 0;

    do {
       if (kbd_waitkey_timeout(&key, 3) != ERR_SUCCESS) {
          firmware_print("Keyboard error");
       } else {
          if (!wait && key.sym != KEYSYM_NONE) {
             wait = 1;
          }
          if (key.sym == KEYSYM_ASCII) {
             if (key.ascii == ASCII_ENTER || num_inputs == 10) {
                if (num_inputs == 0) {
                   bootoption = current_page * num_pagetest +
                                current_row - first_row;
                }
                break;
             } else if (key.ascii - '0' >= 0 || key.ascii - '0' <= 9) {
                bootoption = bootoption * 10 + (key.ascii - '0');
                num_inputs++;
             }
          } else if (key.sym == KEYSYM_UP) {
             if (current_row != first_row) {
                current_row--;
                st->ConOut->SetCursorPosition(st->ConOut, 0, current_row);
             } else {
                print_previous_page();
             }
          } else if (key.sym == KEYSYM_DOWN) {
             uint32_t current_test = current_page * num_pagetest +
                                     current_row - first_row + 1;
             if (current_row != last_row && current_test < num_test) {
                current_row++;
                st->ConOut->SetCursorPosition(st->ConOut, 0, current_row);
             } else {
                print_next_page();
             }
          } else if (key.sym == KEYSYM_PAGE_UP) {
             print_previous_page();
          } else if (key.sym == KEYSYM_PAGE_DOWN) {
             print_next_page();
          }
       }
    } while (wait);

    if (!wait) {
       bootoption = default_option;
    }

    st->ConOut->ClearScreen(st->ConOut);

    return bootoption;
}

/*-- setup_display -------------------------------------------------------------
 *
 *      Prepare for text mode display and print the current page of test list.
 *
 *----------------------------------------------------------------------------*/
int setup_display(void)
{
   EFI_STATUS Status;

   Status = st->ConOut->SetMode(st->ConOut, 0);

   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   st->ConOut->Reset(st->ConOut, TRUE);

   st->ConOut->SetAttribute(st->ConOut,
                            EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK));

   st->ConOut->EnableCursor(st->ConOut, TRUE);

   st->ConOut->QueryMode(st->ConOut, TEXT_MODE, &columns, &rows);

   strcpy(header, "Boot default in 3s or select test/enter test number. ");
   strcat(header, "Press 'Enter' when done. \n");

   first_row = ceil(strlen(header), columns);
   last_row = rows - MARGIN - 1; //row starts from 0
   num_pagetest = rows - first_row - MARGIN;

   max_page = num_test / num_pagetest;
   current_page = bootoption / num_pagetest;

   print_current_page();

   current_row = bootoption % num_pagetest + first_row;
   st->ConOut->SetCursorPosition(st->ConOut, 0, current_row);

   return error_efi_to_generic(Status);
}
