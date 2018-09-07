/*******************************************************************************
 * Copyright (c) 2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_gui.c -- tests gui functionality
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <bootlib.h>
#include <boot_services.h>
#include <fb.h>

#define DEFAULT_PROG_NAME       "test_gui.c32"

#define DEFAULT_WIDTH      1024
#define DEFAULT_HEIGHT     768
#define DEFAULT_DEPTH      32
#define MIN_WIDTH          640
#define MIN_HEIGHT         400
#define MIN_DEPTH          24
#define MARGIN             5        /* Default margin, in pixels */

#define COLOR_BG           BLACK
#define COLOR_TITLE        WHITE
#define COLOR_HRULE        DARK_GRAY

framebuffer_t fb;

static INLINE unsigned int gui_width(void)
{
   return fb.width - 2 * MARGIN;
}


/*-- gui_draw_header -----------------------------------------------------------
 *
 *      Display the console header, which includes a title string and a
 *      horizontal bar.
 *
 * Results
 *      The header height, in pixels
 *----------------------------------------------------------------------------*/
static unsigned int gui_draw_header(void)
{
   const unsigned int h = 2;
   const int x = MARGIN;
   unsigned int w;
   int y;

   y = MARGIN;
   w = gui_width();

   fb_print(&fb, "GUI Test", x, y, w,
            COLOR_BG, COLOR_TITLE, ALIGN_CENTER);

   y += font_height(1) + MARGIN;
   fb_draw_rect(&fb, x, y, w, h, COLOR_HRULE);

   return y + h - MARGIN;
}


/*-- test_gui_init -------------------------------------------------------------
 *
 *      Parse test_gui command line options.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int test_gui_init(int argc, char **argv)
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

/*-- main ----------------------------------------------------------------------
 *
 *      test_gui main function.
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

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = test_gui_init(argc, argv);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "test_gui_init failed: %s\n", error_str[status]);
      return status;
   }

   status = video_set_mode(&fb, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_DEPTH,
                           MIN_WIDTH, MIN_HEIGHT, MIN_DEPTH, false);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "video_set_mode: %s\n", error_str[status]);
      return status;
   }

   status = fbcon_init(&fb, &fb_font, MARGIN, gui_draw_header() + 2 * MARGIN,
                       gui_width(), fb.height, true);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "fbcon_init: %s\n", error_str[status]);
      return status;
   }

   return status;
}
