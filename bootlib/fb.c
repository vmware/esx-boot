/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * fb.c -- Framebuffer management and drawing primitives
 *
 *      This is a very simplistic framebuffer implementation with no kind of
 *      software optimization/acceleration. This would probably need to be
 *      re-written if the framebuffer was used more intensively.
 */

#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <fb.h>
#include <vbe.h>
#include <boot_services.h>
#include <bootlib.h>

#include "font_8x16.c"

font_t fb_font;

/*-- fb_set_pixel --------------------------------------------------------------
 *
 *      Set the (x, y) framebuffer pixel to the given color.
 *
 * Parameters
 *      IN fb:      pointer to the framebuffer info structure
 *      IN x:       horizontal position
 *      IN y:       vertical position
 *      IN fbcolor: framebuffer native color
 *----------------------------------------------------------------------------*/
static void fb_set_pixel(framebuffer_t *fb, int x, int y, uint32_t fbcolor)
{
   uint32_t i, bytes_per_pxl;
   uint8_t *video;

   bytes_per_pxl = fb->depth / 8;
   video = (uint8_t *)fb->addr + y * fb->BytesPerScanLine + x * bytes_per_pxl;

   for (i = 0; i < bytes_per_pxl; i++) {
      video[i] = (uint8_t)(fbcolor >> (i * 8));
   }
}

/*-- fb_draw_char --------------------------------------------------------------
 *
 *      Draw a character into the framebuffer memory.
 *
 * Parameters
 *      IN fb:   pointer to the framebuffer info structure
 *      IN font: pointer to the font
 *      IN x:    horizontal position of the rendering area top-left corner
 *      IN y:    vertical position of the rendering area top-left corner
 *      IN rgba: 32-bit RGBA color
 *----------------------------------------------------------------------------*/
void fb_draw_char(framebuffer_t *fb, const font_t *font, int c, int x, int y,
                  uint32_t rgba)
{
   const unsigned char *glyph;
   uint32_t fbcolor;
   unsigned int i, j;

   fbcolor = rgba_to_native_color32(&fb->pxl, rgba);
   glyph = &font->glyphs[c * font->bytes_per_scanline * font->height];

   for (j = 0; j < font->height; j++) {
      for (i = 0; i < font->width; i++) {
         if (glyph[(font->width - i - 1) / 8] & (1 << ((font->width - i - 1) % 8))) {
            fb_set_pixel(fb, x + i, y + j, fbcolor);
         }
      }
      glyph += font->bytes_per_scanline;
   }
}

/*-- fb_print ------------------------------------------------------------------
 *
 *      Print a one-line string in the GUI. Newlines are ignored and the string
 *      is truncated if it is too large to fit in the given window.
 *
 * Parameters
 *      IN fb:      pointer to the framebuffer info structure
 *      IN str:     pointer to the string to be printed
 *      IN x:       horizontal position in pixels
 *      IN y:       vertical position in pixels
 *      IN width:   window width in pixels
 *      IN bg_rgba: 32-bit RGBA formated background color
 *      IN fg_rgba: 32-bit RGBA formated foreground (text) color
 *      IN align:   horizontal text position
 *----------------------------------------------------------------------------*/
void fb_print(framebuffer_t *fb, const char *str, int x, int y,
              unsigned int width, uint32_t bg_rgba, uint32_t fg_rgba,
              h_align_t align)
{
   int x_char, len;
   const char *s;

   if (bg_rgba != TRANSPARENT) {
      fb_draw_rect(fb, x, y, width, font_height(1), bg_rgba);
   }

   if (width < font_width(1) || str == NULL) {
      return;
   }

   len = 0;

   for (s = str; *s != '\0'; s++) {
      if (isprint(*s)) {
         len++;
      }
      if (font_width(len) > width) {
         len--;
         break;
      }
   }

   switch (align) {
   case ALIGN_RIGHT:
      x_char = x + width - font_width(len);
      break;
   case ALIGN_CENTER:
      x_char = x + (width - font_width(len)) / 2;
      break;
   case ALIGN_LEFT:
   default:
      x_char = x;
      break;
   }

   for (s = str; len > 0; s++) {
      if (isprint(*s)) {
         fb_draw_char(fb, &fb_font, *s, x_char, y, fg_rgba);
         x_char += font_width(1);
         len--;
      }
   }
}

/*-- fb_crop_invisible ---------------------------------------------------------
 *
 *      Crop the invisible part of a rectangle region. This functions is used to
 *      avoid drawing out of the screen boundaries.
 *
 * Parameters
 *      IN  fb:     pointer to the framebuffer info structure
 *      IN  x:      horizontal position of the initial rectangle
 *      IN  y:      vertical position of the initial rectangle
 *      IN  width:  the horizontal length in pixels
 *      IN  height: the vertical length in pixels
 *      OUT x:      horizontal position of the cropped rectangle
 *      OUT y:      vertical position of the cropped rectangle
 *      OUT width:  horizontal length in pixels of the cropped rectangle
 *      OUT height: updated vertical length in pixels of the cropped rectangle
 *
 * Results
 *      0 if the rectangle has been cropped, or 1 if the initial rectangle is
 *      completely out of the visible area.
 *----------------------------------------------------------------------------*/
static int fb_crop_invisible(framebuffer_t *fb, int *x, int *y,
                             unsigned int *width, unsigned int *height)
{
   if (*x + (int)*width < 0
    || *y + (int)*height < 0
    || *x >= (int)fb->width
    || *y >= (int)fb->height) {
      /* Out of the visible window */
      return 1;
   }

   if (*x < 0) {
      *width += *x;
      *x = 0;
   }
   if (*x + *width >= fb->width) {
      *width = fb->width - *x;
   }

   if (*y < 0) {
      *height += *y;
      *y = 0;
   }
   if (*y + *height >= fb->height) {
      *height = fb->height - *y;
   }

   return 0;
}

/*-- fb_draw_rect --------------------------------------------------------------
 *
 *      Draw a rectangle into the framebuffer. The rectangle is filled up with
 *      the given RGBA color. If the rectangle is too big or out of the display
 *      window, only the visible part is drawn.
 *
 * Parameters
 *      IN fb:     pointer to the framebuffer info structure
 *      IN x:      horizontal position of the top-left corner
 *      IN y:      vertical position of the top-left corner
 *      IN width:  horizontal length in pixels
 *      IN height: vertical length in pixels
 *      IN rgba:   32-bit RGBA filling color
 *----------------------------------------------------------------------------*/
void fb_draw_rect(framebuffer_t *fb, int x, int y, unsigned int width,
                  unsigned int height, uint32_t rgba)
{
   uint32_t fbcolor;
   uint32_t i, j, bytes_per_pxl;
   uint8_t *video;

   if (fb_crop_invisible(fb, &x, &y, &width, &height) == 0) {
      fbcolor = rgba_to_native_color32(&fb->pxl, rgba);
      bytes_per_pxl = fb->depth / 8;
      video = (uint8_t *)fb->addr + y * fb->BytesPerScanLine + x * bytes_per_pxl;

      while (height--) {
         for (j = 0; j < width; j++) {
            for (i = 0; i < bytes_per_pxl; i++) {
               video[j * bytes_per_pxl + i] = (uint8_t)(fbcolor >> (i * 8));
            }
         }
         video += fb->BytesPerScanLine;
      }
   }
}

/*-- fb_scroll_up --------------------------------------------------------------
 *
 *      Scroll up the framebuffer display.
 *
 * Parameters
 *      IN fb:     pointer to the framebuffer info structure
 *      IN nlines: number of line to scroll up
 *----------------------------------------------------------------------------*/
void fb_scroll_up(framebuffer_t *fb, unsigned int nlines)
{
   uint8_t *video;
   size_t size;

   video = fb->addr;
   size = nlines * fb->BytesPerScanLine;

   memmove(video, video + size, fb->size - size);
   memset(video + fb->size - size, 0, size);
}

/*-- fb_load_font --------------------------------------------------------------
 *
 *      Font initialization. If gzipped, the font is extracted into a freshly
 *      allocated buffer.
 *
 * Parameters
 *      IN data:               pointer to the (compressed) font data
 *      IN datalen:            size of the font data, in bytes
 *      IN width:              glyphs width in pixels
 *      IN height:             glyphs height in pixels
 *      IN bytes_per_scanline: number of bytes per glyph scanline
 *      IN font:               pointer to the font info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int fb_load_font(const void *data, size_t datalen, unsigned int width,
                 unsigned int height, unsigned int bytes_per_scanline,
                 font_t *font)
{
   void *glyphs;
   size_t size;
   int status;

   status = gzip_extract(data, datalen, &glyphs, &size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (size != (bytes_per_scanline * height * 256)) {
      free(glyphs);
      return ERR_INVALID_PARAMETER;
   }

   memset(font, 0, sizeof (font_t));
   font->width = width;
   font->height = height;
   font->bytes_per_scanline = bytes_per_scanline;
   font->glyphs = glyphs;

   return ERR_SUCCESS;
}

/*-- fb_clear ------------------------------------------------------------------
 *
 *      Clear the entire framebuffer.
 *----------------------------------------------------------------------------*/
void fb_clear(framebuffer_t *fb)
{
   memset(fb->addr, 0, fb->size);
}

/*-- fb_init -------------------------------------------------------------------
 *
 *      Initialize the framebuffer for the current VBE mode.
 *
 * Parameters
 *      IN vbe:     pointer to the vbe info structure
 *      IN fb_addr: framebuffer address
 *      IN fb:      pointer to the framebuffer info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int fb_init(vbe_mode_t *vbe, uintptr_t fb_addr, framebuffer_t *fb)
{
   int status;

   if (vbe == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if (vbe->BitsPerPixel % 8 || vbe->BitsPerPixel > 32) {
      /* Depth not supported */
      return ERR_INVALID_PARAMETER;
   }

   memset(fb, 0, sizeof (framebuffer_t));
   fb->addr = UINT_TO_PTR(fb_addr);
   fb->size = (size_t)vbe->YResolution * (size_t)vbe->BytesPerScanLine;
   fb->width = vbe->XResolution;
   fb->height = vbe->YResolution;
   fb->depth = vbe->BitsPerPixel;
   fb->BytesPerScanLine = vbe->BytesPerScanLine;
   fb->pxl.RedSize = vbe->RedMaskSize;
   fb->pxl.RedOffset = vbe->RedFieldPosition;
   fb->pxl.GreenSize = vbe->GreenMaskSize;
   fb->pxl.GreenOffset = vbe->GreenFieldPosition;
   fb->pxl.BlueSize = vbe->BlueMaskSize;
   fb->pxl.BlueOffset = vbe->BlueFieldPosition;
   fb->pxl.RsvdSize = vbe->RsvdMaskSize;
   fb->pxl.RsvdOffset = vbe->RsvdFieldPosition;

   status = fb_load_font(bsd_font_8x16, sizeof (bsd_font_8x16),
                         8, 16, 1, &fb_font);

   if (status == ERR_SUCCESS) {
      fb_clear(fb);
   }

   return status;
}
