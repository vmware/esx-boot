/*******************************************************************************
 * Copyright (c) 2008-2011,2014 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * gui.c -- Safeboot graphical user interface
 */

#include <stdio.h>
#include <ctype.h>
#include <fb.h>
#include <bootlib.h>
#include <boot_services.h>

#include "safeboot.h"

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
#define COLOR_TEXT         GRAY
#define COLOR_KEY          GOLD
#define COLOR_INPUT        WHITE

#define ASCII_ENTER        0x0d

static framebuffer_t *fb;           /* Framebuffer properties */

static INLINE unsigned int gui_width(void)
{
   return fb->width - 2 * MARGIN;
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

   fb_print(fb, "VMware Hypervisor Recovery", x, y, w,
            COLOR_BG, COLOR_TITLE, ALIGN_CENTER);

   y += font_height(1) + MARGIN;
   fb_draw_rect(fb, x, y, w, h, COLOR_HRULE);

   return y + h - MARGIN;
}

/*-- gui_rollback --------------------------------------------------------------
 *
 *      Request users for a roll back confirmation:
 *         <Y> Yes, I want to roll back
 *         <N> No, I do not want to roll back
 *
 * Results
 *      True if roll back is confirmed, False otherwhise.
 *----------------------------------------------------------------------------*/
bool gui_rollback(void)
{
   const int x = MARGIN;
   int y;
   unsigned int w;
   key_code_t key;
   bool status;

   y = fb->height - font_height(2) - MARGIN;
   w = gui_width();

   Log(LOG_WARNING,
       "CURRENT DEFAULT HYPERVISOR WILL BE REPLACED PERMANENTLY.\n");
   Log(LOG_WARNING, "DO YOU REALLY WANT TO ROLL BACK?\n");

   fb_print(fb, "< : Roll back>", x, y, w, COLOR_BG, COLOR_TEXT, ALIGN_LEFT);
   fb_print(fb, " Y", x, y, w, TRANSPARENT, COLOR_KEY, ALIGN_LEFT);

   fb_print(fb, "< : Cancel>", x, y, w, TRANSPARENT, COLOR_TEXT, ALIGN_RIGHT);
   fb_print(fb, " N         ", x, y, w, TRANSPARENT, COLOR_KEY, ALIGN_RIGHT);

   while (1) {
      if (kbd_waitkey(&key) != ERR_SUCCESS) {
         Log(LOG_WARNING, "Keyboard error\n");
      } else if (key.sym == KEYSYM_ASCII) {
         if (key.ascii == 'y' || key.ascii == 'Y') {
            status = true;
            break;
         } else if (key.ascii == 'n' || key.ascii == 'N') {
            status = false;
            break;
         }
      }
   }

   fb_draw_rect(fb, x, y - font_height(1), w, font_height(2), COLOR_BG);

   return status;
}

/*-- gui_resume_default_boot ---------------------------------------------------
 *
 *      Display a 10-second countdown before safeboot automatically resumes
 *      booting from the current default boot bank. Users can interrupt the
 *      countdown and boot immediately by pressing <ENTER>.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
#define RESUME_TIMEOUT 10
int gui_resume_default_boot(void)
{
   const int x = MARGIN;
   unsigned int w;
   key_code_t key;
   int n, y;
   char *msg;

   y = fb->height - font_height(2) - MARGIN;
   w = gui_width();

   fb_print(fb, "<     : Boot default hypervisor>", x, y, w,
            COLOR_BG, COLOR_TEXT, ALIGN_LEFT);
   fb_print(fb, " ENTER", x, y, w, TRANSPARENT, COLOR_KEY, ALIGN_LEFT);

   y += font_height(1);

   for (n = RESUME_TIMEOUT; n > 0; n--) {
      if (asprintf(&msg, "Booting default hypervisor in %d second%s",
                   n, (n > 1) ? "s..." : "... ") == -1) {
         return ERR_OUT_OF_RESOURCES;
      }
      fb_print(fb, msg, x, y, font_width(strlen(msg)),
               COLOR_BG, COLOR_INPUT, ALIGN_LEFT);
      sys_free(msg);

      if (kbd_waitkey_timeout(&key, 1) != ERR_SUCCESS) {
         Log(LOG_WARNING, "Keyboard error\n");
      } else if (key.sym == KEYSYM_ASCII && key.ascii == ASCII_ENTER) {
         break;
      } else if (key.sym != KEYSYM_NONE) {
         n = RESUME_TIMEOUT;
      }
   }

   fb_draw_rect(fb, x, y - font_height(1), w, font_height(2), COLOR_BG);

   return ERR_SUCCESS;
}

/*-- gui_init ------------------------------------------------------------------
 *
 *      Graphical interface initialization.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int gui_init(void)
{
   int status;

   fb = &safeboot.fb;

   status = video_set_mode(fb, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_DEPTH,
                           MIN_WIDTH, MIN_HEIGHT, MIN_DEPTH, false);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = fbcon_init(fb, &fb_font, MARGIN, gui_draw_header() + 2 * MARGIN,
                       gui_width(), fb->height, safeboot.verbose);
   return status;
}
