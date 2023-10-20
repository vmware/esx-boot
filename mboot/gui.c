/*******************************************************************************
 * Copyright (c) 2008-2011,2013-2014,2017,2022-2023 VMware, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fb.h>
#include <boot_services.h>

#include "mboot.h"

#define DEFAULT_WIDTH      1024
#define DEFAULT_HEIGHT     768
#define DEFAULT_DEPTH      32
#define MIN_WIDTH          640
#define MIN_HEIGHT         400
#define MIN_DEPTH          24
#define MARGIN             5                     /* Default margin, in pixels */

#define COLOR_BG           BLACK
#define COLOR_TITLE        WHITE
#define COLOR_BGPROGRESS   DARK_GRAY
#define COLOR_FGPROGRESS   GOLD
#define COLOR_TEXT         GRAY
#define COLOR_KEY          GOLD
#define COLOR_INPUT        WHITE
#define COLOR_PROMPT       GRAY

#define ASCII_BACKSPACE    0x08
#define ASCII_ENTER        0x0d
#define ASCII_ESCAPE       0x1b
#define ASCII_DELETE       0x7f

typedef enum {
   RENDER_REFRESH,     /* Only draw changin pixels */
   RENDER_ALL          /* (Re)draw all */
} gui_render_t;

static framebuffer_t *fb;           /* Framebuffer properties */
static bool title_changed = false;

static INLINE unsigned int gui_width(void)
{
   return fb->width - 2 * MARGIN;
}

/*-- gui_set_title -------------------------------------------------------------
 *
 *      Set the bootloader title string.
 *
 * Parameters
 *      IN title: pointer to the new title string
 *----------------------------------------------------------------------------*/
void gui_set_title(const char *title)
{
   size_t i, len;

   if (title == NULL || title[0] == '\0') {
      return;
   }

   len = MIN(strlen(title), TITLE_MAX_LEN - 1);

   for (i = 0; i < len; i++) {
      boot.title[i] = title[i];
   }

   boot.title[len] = '\0';

   title_changed = true;
}

/*-- gui_draw_header -----------------------------------------------------------
 *
 *      Display the console header, which includes a title string and a progress
 *      bar.
 *
 * Parameters
 *      IN rendering: RENDER_ALL or RENDER_REFRESH
 *
 * Results
 *      The header height, in pixels
 *----------------------------------------------------------------------------*/
static unsigned int gui_draw_header(gui_render_t rendering)
{
   static unsigned int old_progress = 0;
   unsigned int i, progress, h, w;
   const char *title;
   int x, y;

   x = MARGIN;
   y = MARGIN;
   w = gui_width();

   if (title_changed || rendering == RENDER_ALL) {
      title = (boot.title[0] != '\0') ? boot.title : "Loading operating system";
      fb_print(fb, title, x, y, w, COLOR_BG, COLOR_TITLE, ALIGN_CENTER);
      title_changed = false;
   }

   if (boot.modules == 0) {
      progress = 0;
   } else if (boot.load_size > 0) {
      progress = (unsigned int)((boot.load_offset * w) / boot.load_size);
   } else {
      for (i = 0; i < boot.modules_nr; i++) {
         if (!boot.modules[i].is_loaded) {
            break;
         }
      }
      progress = (i == boot.modules_nr) ? w : (w / boot.modules_nr) * i;
   }

   h = MAX(1, w / 115);
   y += font_height(1) + MARGIN;

   if (rendering == RENDER_ALL || boot.modules_nr == 0) {
      fb_draw_rect(fb, x + progress, y, w - progress, h, COLOR_BGPROGRESS);
   }
   if (progress != old_progress) {
      if (progress > 0) {
         fb_draw_rect(fb, x, y, progress, h, COLOR_FGPROGRESS);
      }
      old_progress = progress;
   }

   return y + h - MARGIN;
}

/*-- gui_refresh ---------------------------------------------------------------
 *
 *      Callback to be called periodically in order to refresh the GUI. Writing
 *      in the framebuffer makes this function very slow. Then the gui_refresh()
 *      callback should only be called when it is really necessary.
 *----------------------------------------------------------------------------*/
void gui_refresh(void)
{
   if (!boot.headless) {
      gui_draw_header(RENDER_REFRESH);
   }
}

/*-- gui_string_edit -----------------------------------------------------------
 *
 *      Allow user to edit a string in a text field. The buffer containing the
 *      string must have been allocated with sys_malloc() and must be exactly
 *      (strlen(str) + 1) large. The string is reallocated if needed.
 *
 *      The input string is modified in place and is not freed on errors or when
 *      <ESC> is pressed.
 *
 *      <LEFT>  <RIGHT>: move the cursor one character to the left/right
 *      <HOME>  <END>:   move the cursor at the beginning/end of the line
 *      <BKSPC> <DEL>:   delete one character before/after the cursor
 *      <ENTER> <ESC>:   Validate/abort edition
 *
 * Parameters
 *      IN  x:   horizontal text field position, in pixels
 *      IN  y:   vertical text field position, in pixels
 *      IN  w:   text field width, in pixels
 *      IN  str: pointer to the input string
 *      OUT str: pointer to the output string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int gui_string_edit(int x, int y, unsigned int w, char **str)
{
   size_t buflen, cmdlen, cursor, offset, width;
   char *buffer, *tmp;
   key_code_t key;
   int status;

   fb_print(fb, ">", x, y, font_width(2), COLOR_BG, COLOR_PROMPT, ALIGN_LEFT);
   x += font_width(2);
   w -= font_width(2);
   width = w / font_width(1);

   buffer = *str;
   cmdlen = strlen(buffer);
   buflen = cmdlen + 1;
   offset = (cmdlen > width) ? cmdlen - width : 0;
   cursor = cmdlen;

   for ( ; ; ) {
      fb_print(fb, buffer + offset, x, y, w, COLOR_BG, COLOR_INPUT, ALIGN_LEFT);

      fb_draw_rect(fb, x + font_width(cursor - offset), y, 2, font_height(1),
                   COLOR_PROMPT);

      status = kbd_waitkey(&key);
      if (status != ERR_SUCCESS) {
         *str = buffer;
         Log(LOG_WARNING, "Keyboard error");
         return status;
      }

      switch (key.sym) {
         case KEYSYM_RIGHT:
            if (cursor < cmdlen) {
               if (cursor == offset + width) {
                  offset++;
               }
               cursor++;
            }
            break;
         case KEYSYM_LEFT:
            if (cursor > 0) {
               if (cursor == offset) {
                  offset--;
               }
               cursor--;
            }
            break;
         case KEYSYM_HOME:
            cursor = 0;
            offset = 0;
            break;
         case KEYSYM_END:
            cursor = cmdlen;
            offset = (cmdlen > width) ? cmdlen - width : 0;
            break;
         case KEYSYM_ASCII:
            switch (key.ascii) {
               case ASCII_DELETE:
                  if (cmdlen > 0 && cursor < cmdlen) {
                     delete_char(buffer, cursor);
                     cmdlen--;
                  }
                  break;
               case ASCII_BACKSPACE:
                  if (cursor > 0) {
                     if (cursor == offset) {
                        offset--;
                     }
                     delete_char(buffer, --cursor);
                     cmdlen--;
                  }
                  break;
               case ASCII_ENTER:
                  *str = buffer;
                  return ERR_SUCCESS;
               case ASCII_ESCAPE:
                  *str = buffer;
                  return ERR_ABORTED;
               default:
                  if (isprint(key.ascii)) {
                     if (cursor == offset + width) {
                        offset++;
                     }
                     if (cmdlen + 1 == buflen) {
                        tmp = buffer;
                        buffer = sys_realloc(tmp, buflen, buflen + 128);
                        if (buffer == NULL) {
                           *str = tmp;
                           return ERR_OUT_OF_RESOURCES;
                        }
                        buflen += 128;
                     }

                     insert_char(buffer, key.ascii, cursor++);
                     cmdlen++;
                  }
            }
         default:
            break;
      }
   }
}

/*-- edit_kernel_options -------------------------------------------------------
 *
 *      Allow users to edit the kernel boot options.
 *
 *      <ENTER> Validates the new boot options and starts booting.
 *      <ESC>   Resets the default kernel boot options and returns to the
 *              previous countdown.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int edit_kernel_options(void)
{
   int x, y, status;
   unsigned int w;
   char *options;

   if (boot.modules[0].options != NULL) {
      options = strdup(boot.modules[0].options);
      status = (options != NULL) ? ERR_SUCCESS : ERR_OUT_OF_RESOURCES;
   } else {
      status = str_alloc(0, &options);
   }

   if (status != ERR_SUCCESS) {
      return status;
   }

   x = MARGIN;
   y = fb->height - font_height(2) - MARGIN;
   w = gui_width();

   fb_print(fb, "<     : Apply options and boot>", x, y, w,
            COLOR_BG, COLOR_TEXT, ALIGN_LEFT);
   fb_print(fb, " ENTER", x, y, w, TRANSPARENT, COLOR_KEY, ALIGN_LEFT);
   fb_print(fb, "<   : Cancel>", x, y, w, TRANSPARENT, COLOR_TEXT, ALIGN_RIGHT);
   fb_print(fb, " ESC         ", x, y, w, TRANSPARENT, COLOR_KEY, ALIGN_RIGHT);

   y += font_height(1);

   status = gui_string_edit(x, y, w, &options);
   fb_draw_rect(fb, x, y, w, font_height(1), COLOR_BG);
   if (status != ERR_SUCCESS) {
      sys_free(options);
      return status;
   }

   sys_free(boot.modules[0].options);
   if (options[0] == '\0') {
      sys_free(options);
      boot.modules[0].options = NULL;
   } else {
      boot.modules[0].options = options;
   }

   return ERR_SUCCESS;
}

/*-- gui_edit_kernel_options ---------------------------------------------------
 *
 *      Allow users to interrupt booting in order to edit the kernel boot
 *      options or to enter into recovery mode. The bootloader automatically
 *      boots with default kernel options (as passed in the boot configuration
 *      file and on the command line) it not interrupted before a 5-second
 *      countdown has expired.
 *
 *      <SHIFT+O> Allows user to edit the kernel boot options.
 *
 *      <SHIFT+R> Enters the recovery mode (only if a recovery command was
 *                passed on mboot's command line with the '-R COMMAND' option).
 *      
 *      <SHIFT+V> Turns on verbose logging as with the -V command line option,
 *                if not already on.
 *
 *      <SHIFT+S> Turns on verbose logging to serial as with the -S1 command
 *                line option, if not already on.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int gui_edit_kernel_options(void)
{
   bool shift_r;
   int x, y, status;
   unsigned int w;
   key_code_t key;
   char msg[31];
   char n;

   gui_refresh();

   x = MARGIN;
   y = fb->height - font_height(2) - MARGIN;
   w = gui_width();
   shift_r = (boot.recovery_cmd != NULL);

 countdown:
   status = ERR_SUCCESS;

   fb_print(fb, "<     : Boot>", x, y, w, COLOR_BG, COLOR_TEXT, ALIGN_LEFT);
   fb_print(fb, " ENTER", x, y, w, TRANSPARENT, COLOR_KEY, ALIGN_LEFT);

   fb_print(fb, "<       : Edit boot options>", x, y, w,
            TRANSPARENT, COLOR_TEXT, ALIGN_RIGHT);
   fb_print(fb, " SHIFT+O                    ", x, y, w,
            TRANSPARENT, COLOR_KEY, ALIGN_RIGHT);

   y += font_height(1);

   if (shift_r) {
      fb_print(fb, "<       : Recovery mode>    ", x, y, w,
               COLOR_BG, COLOR_TEXT, ALIGN_RIGHT);
      fb_print(fb, " SHIFT+R                    ", x, y, w,
               TRANSPARENT, COLOR_KEY, ALIGN_RIGHT);
   }

   for (n = boot.timeout; n > 0; n--) {
      snprintf(msg, 31, "Automatic boot in %c second%s...",
               '0' + n, (n > 1) ? "s" : "");
      fb_print(fb, msg, x, y, font_width(30),
               COLOR_BG, COLOR_INPUT, ALIGN_LEFT);

      if (kbd_waitkey_timeout(&key, 1) != ERR_SUCCESS) {
         Log(LOG_WARNING, "Keyboard error");
      } else if (key.sym == KEYSYM_ASCII && key.ascii == ASCII_ENTER) {
         break;
      } else if (key.sym == KEYSYM_ASCII && key.ascii == 'O') {
         status = edit_kernel_options();
         if (status == ERR_ABORTED) {
            y -= font_height(1);
            goto countdown;
         }
         break;
      } else if (key.sym == KEYSYM_ASCII && key.ascii == 'R' && shift_r) {
         return ERR_ABORTED;
      } else if (key.sym == KEYSYM_ASCII && key.ascii == 'V' && !boot.verbose) {
         Log(LOG_INFO, "Shift+V pressed: Enabling verbose logging to screen");
         boot.verbose = true;
         fbcon_set_verbosity(boot.verbose);
      } else if (key.sym == KEYSYM_ASCII && key.ascii == 'S' && !boot.serial) {
         Log(LOG_INFO, "Shift+S pressed: Enabling serial log to COM1");
         boot.serial =
            serial_log_init(DEFAULT_SERIAL_COM,
                            DEFAULT_SERIAL_BAUDRATE) == ERR_SUCCESS;
      } else if (key.sym == KEYSYM_ASCII && key.ascii == 'U' && !boot.no_rts) {
         Log(LOG_INFO, "Shift+U pressed: Disabling UEFI runtime services");
         boot.no_rts = true;
      } else if (key.sym != KEYSYM_NONE) {
         n = 6;
      }
   }

   fb_draw_rect(fb, x, y - font_height(1), w, font_height(2), COLOR_BG);

   return status;
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
   unsigned int h;
   int status, y;

   fb = &boot.fb;

   status = video_set_mode(fb, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_DEPTH,
                           MIN_WIDTH, MIN_HEIGHT, MIN_DEPTH, boot.debug);
   if (status != ERR_SUCCESS) {
      return status;
   }

   y = gui_draw_header(RENDER_ALL) + 2 * MARGIN;
   h = fb->height - y - MARGIN;

   return fbcon_init(fb, &fb_font, MARGIN, y, gui_width(), h, boot.verbose);
}

/*-- gui_text ------------------------------------------------------------------
 *
 *      Switch the GUI to VGA text mode.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int gui_text(void)
{
   int status;

   status = video_set_text_mode();
   if (status != ERR_SUCCESS) {
      return status;
   }

   fbcon_shutdown();

   return ERR_SUCCESS;
}

/*-- gui_resize ----------------------------------------------------------------
 *
 *      Resize the video mode and the graphical interface to the given width,
 *      height and depth. If the new video mode cannot be setup, then the GUI
 *      toggles to standard VGA text mode.
 *
 * Parameters
 *      IN width:         preferred horizontal resolution, in pixels
 *      IN height:        preferred vertical resolution, in pixels
 *      IN depth:         preferred color depth, in bits per pixel
 *      IN min_width:     minimum horizontal resolution, in pixels
 *      IN min_height:    minimum vertical resolution, in pixels
 *      IN min_depth:     minimum color depth, in bits per pixel

 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int gui_resize(unsigned int width, unsigned int height, unsigned int depth,
               unsigned int min_width, unsigned int min_height,
               unsigned int min_depth)
{
   int status;

   /*
    * Note: Don't optimize here by checking whether the requested values match
    * what we already think the display is at; just call video_set_mode
    * unconditionally.  This works around rude/broken firmware that thinks it
    * still owns the display and changes the values out from under us.  See PR
    * 1865804.
    */
   status = video_set_mode(fb, width, height, depth,
                           min_width, min_height, min_depth, false);
   if (status != ERR_SUCCESS) {
      return status;
   }

   fbcon_reset();

   return ERR_SUCCESS;
}

/*-- gui_exit ------------------------------------------------------------------
 *
 *      Print a countdown to exiting mboot.
 *
 * Results
 *      True if mboot should exit so the calling program can handle the error.
 *      For example, by rebooting the machine.
 *----------------------------------------------------------------------------*/
bool gui_exit(int timeout)
{
   bool status = true;
   unsigned int w;
   key_code_t key;
   char msg[64];
   int x, y, n;

   x = MARGIN;
   y = fb->height - font_height(2) - MARGIN;
   w = gui_width();

   fb_print(fb, "<     : Exit immediately>",
            x, y, w, COLOR_BG, COLOR_TEXT, ALIGN_LEFT);
   fb_print(fb, " ENTER", x, y, w, TRANSPARENT, COLOR_KEY, ALIGN_LEFT);

   fb_print(fb, "<             : Cancel>", x, y, w,
            TRANSPARENT, COLOR_TEXT, ALIGN_RIGHT);
   fb_print(fb, " ANY OTHER KEY         ", x, y, w,
            TRANSPARENT, COLOR_KEY, ALIGN_RIGHT);

   for (n = timeout; n >= 0; n--) {
      fb_draw_rect(fb, x, y + font_height(1), w, font_height(1), COLOR_BG);

      snprintf(msg, sizeof(msg), "Exiting in %d second%s...",
               n, (n > 1) ? "s" : "");
      fb_print(fb, msg, x, y + font_height(1), font_width(sizeof(msg)),
               COLOR_BG, COLOR_INPUT, ALIGN_LEFT);

      if (kbd_waitkey_timeout(&key, 1) != ERR_SUCCESS) {
         Log(LOG_WARNING, "Keyboard error");
      } else if (key.sym == KEYSYM_ASCII && key.ascii == ASCII_ENTER) {
         break;
      } else if (key.sym != KEYSYM_NONE) {
         status = false;
         break;
      }
   }

   fb_draw_rect(fb, x, y, w, font_height(2), COLOR_BG);

   return status;
}
