/*******************************************************************************
 * Copyright (c) 2008-2017 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * fbcon.c -- Framebuffer console
 *
 *      This console is only used for logging output. No input is supported.
 */

#include <ctype.h>
#include <syslog.h>
#include <fb.h>
#include <boot_services.h>
#include <bootlib.h>

static framebuffer_t *fb;           /* Framebuffer properties */
static font_t *font;                /* Console font */
static unsigned int xcurs, ycurs;   /* Cursor position */
static unsigned int rows, columns;  /* Console geometry */
static uint32_t text_color;         /* Current text color */
static int fbcon_x, fbcon_y;        /* Console position */

/*-- fbcon_putc ---------------------------------------------------------------
 *
 *      Print a character on the framebuffer console.
 *
 * Parameters
 *      IN c: character to be printed
 *----------------------------------------------------------------------------*/
static void fbcon_putc(char c)
{
   static bool scrollup = false;

   if (scrollup) {
      /* Unfortunatly, scrolling is too slow, so we just clear the screen. */
      fbcon_clear();
      scrollup = false;
   }

   if (c == '\n') {
      xcurs = 0;
      if (ycurs >= rows - 1) {
         scrollup = true;
      } else {
         ycurs++;
      }
      return;
   }

   if (xcurs >= columns) {
      if (ycurs >= rows - 1) {
         fbcon_clear();
      } else {
         xcurs = 0;
         ycurs++;
      }
   }

   if (isprint(c)) {
      fb_draw_char(fb, font, c, fbcon_x + xcurs * font->width,
                   fbcon_y + ycurs * font->height, text_color);
      xcurs++;
   }
}

/*-- fbcon_print_syslog_message ------------------------------------------------
 *
 *      Print syslog formated messages on the framebuffer console. "<n>" prefix
 *      is not printed. Instead, the message severity is indicated by a
 *      dedicated color.
 *
 * Parameters
 *      IN str: pointer to the string to be printed
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int fbcon_print_syslog_message(const char *str)
{
   int level;

   while (*str != '\0') {
      if (xcurs == 0 && syslog_get_message_level(str, &level) == ERR_SUCCESS) {
         str += 3;
         if (*str == '\0') {
            break;
         }

         switch (level) {
            case LOG_EMERG:
            case LOG_ALERT:
            case LOG_CRIT:
            case LOG_ERR:
               text_color = RED;
               break;
            case LOG_WARNING:
               text_color = ORANGE;
               break;
            case LOG_DEBUG:
               text_color = GRAY;
               break;
            case LOG_NOTICE:
            case LOG_INFO:
            default:
               text_color = LIGHT_GRAY;
               break;
         }
      }

      fbcon_putc(*str);
      str++;
   }

   return ERR_SUCCESS;
}

/*-- fbcon_reset ---------------------------------------------------------------
 *
 *      Reset the cursor position to the top left corner.
 *----------------------------------------------------------------------------*/
void fbcon_reset(void)
{
   ycurs = 0;
   xcurs = 0;
}

/*-- fbcon_clear ---------------------------------------------------------------
 *
 *      Clear the framebuffer console and reset the cursor position.
 *----------------------------------------------------------------------------*/
void fbcon_clear(void)
{
   fb_draw_rect(fb, fbcon_x, fbcon_y, columns * font->width,
                rows * font->height, 0);

   fbcon_reset();
}

/*-- fbcon_init ----------------------------------------------------------------
 *
 *      Initialize and enable the framebuffer console.
 *
 * Parameters
 *      IN fbinfo:    pointer to the framebuffer information structure
 *      IN cons_font: pointer to the font info structure
 *      IN x:         horizontal position of the console (in pixels)
 *      IN y:         vertical position of the console (in pixels)
 *      IN width:     console horizontal length (in pixels)
 *      IN height:    console vertical length (in pixels)
 *      IN verbose:   show LOG_DEBUG messages on framebuffer console
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side effects
 *      Stop redirecting the logs to the firmware console since they are ready
 *      to be displayed on the framebuffer console.
 *----------------------------------------------------------------------------*/
int fbcon_init(framebuffer_t *fbinfo, font_t *cons_font, int x, int y,
               unsigned int width, unsigned int height, bool verbose)
{
   int ret;

   if (fbinfo == NULL || cons_font == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if ((width < cons_font->width) || (height < cons_font->height)) {
      return ERR_INVALID_PARAMETER;
   }

   fb = fbinfo;
   font = cons_font;

   rows = height / font->height;
   columns = width / font->width;
   fbcon_x = x;
   fbcon_y = y;
   fbcon_reset();

   log_unsubscribe(firmware_print);

   ret = log_subscribe(fbcon_print_syslog_message,
                       verbose ? LOG_DEBUG : LOG_INFO);
   
   return ret;
}

/*-- fbcon_shutdown ------------------------------------------------------------
 *
 *      Disable the framebuffer console.
 *----------------------------------------------------------------------------*/
void fbcon_shutdown(void)
{
   log_unsubscribe(fbcon_print_syslog_message);
}

/*-- fbcon_set_verbosity -------------------------------------------------------
 *
 *      Turn verbosity off or on for the framebuffer console
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int fbcon_set_verbosity(bool verbose)
{
   log_unsubscribe(fbcon_print_syslog_message);
   return log_subscribe(fbcon_print_syslog_message,
                        verbose ? LOG_DEBUG : LOG_INFO);
}
