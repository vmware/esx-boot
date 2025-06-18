/*******************************************************************************
 * Copyright (c) 2008-2016 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * fb.h -- Frame buffer management and drawing primitives header
 */

#ifndef FB_H_
#define FB_H_

#include <compat.h>
#include <vbe.h>

/*
 * Generic 32-bit RGBA pixel
 */
typedef struct {
   uint8_t RedSize;
   uint8_t RedOffset;
   uint8_t GreenSize;
   uint8_t GreenOffset;
   uint8_t BlueSize;
   uint8_t BlueOffset;
   uint8_t RsvdSize;
   uint8_t RsvdOffset;
} pixel32_t;

/*
 * Generic 32-bit RGBA color
 */
#define RGB(_r_, _g_, _b_)       \
   (((uint32_t)(_r_) << 24) +    \
    ((uint32_t)(_g_) << 16) +    \
    ((uint32_t)(_b_) << 8))

#define RED_COMPONENT(_rgba_)    ((uint8_t)((_rgba_) >> 24))
#define GREEN_COMPONENT(_rgba_)  ((uint8_t)((_rgba_) >> 16))
#define BLUE_COMPONENT(_rgba_)   ((uint8_t)((_rgba_) >> 8))
#define ALPHA_COMPONENT(_rgba_)  ((uint8_t)(_rgba_))

#define BLACK        RGB(0,    0,    0)
#define DARK_GRAY    RGB(0x40, 0x40, 0x40)
#define GRAY         RGB(0x80, 0x80, 0x80)
#define LIGHT_GRAY   RGB(0xb0, 0xb0, 0xb0)
#define WHITE        RGB(0xff, 0xff, 0xff)
#define RED          RGB(0xff, 0,    0)
#define GREEN        RGB(0,    0xff, 0)
#define BLUE         RGB(0x80, 0xb0, 0xff)
#define DARK_BLUE    RGB(0,    0,    0xff)
#define YELLOW       RGB(0xff, 0xff, 0)
#define MAGENTA      RGB(0xff, 0,    0xff)
#define CYAN         RGB(0,    0xff, 0xff)
#define ORANGE       RGB(0xff, 0x80, 0)
#define PINK         RGB(0xff, 0,    0x80)
#define GOLD         RGB(0xff, 0xcc, 0x00)
#define TRANSPARENT  0x000000ff

#define MAKE_COMPONENT(_component_, _depth_, _position_)             \
   (((_component_) >> (8 - (_depth_))) << (_position_))

static INLINE uint32_t rgba_to_native_color32(pixel32_t *p, uint32_t rgba)
{
   return MAKE_COMPONENT(RED_COMPONENT(rgba), p->RedSize, p->RedOffset)
        | MAKE_COMPONENT(GREEN_COMPONENT(rgba), p->GreenSize, p->GreenOffset)
        | MAKE_COMPONENT(BLUE_COMPONENT(rgba), p->BlueSize, p->BlueOffset)
        | MAKE_COMPONENT(ALPHA_COMPONENT(rgba), p->RsvdSize, p->RsvdOffset);
}

typedef struct {
   void *addr;                      /* Framebuffer base address */
   size_t size;                     /* Framebuffer total size (in bytes) */
   uint32_t width;                  /* Horizontal resolution in pixels */
   uint32_t height;                 /* Vertical resolution in pixels */
   uint32_t depth;                  /* Number of bits per pixel */
   uint32_t BytesPerScanLine;       /* Number of bytes per scanline */
   pixel32_t pxl;                   /* Pixel layout */
} framebuffer_t;

typedef struct {
   const unsigned char *glyphs;     /* Font bitmap */
   uint32_t width;                  /* Glyphs rendering width in pixels */
   uint32_t height;                 /* Glyphs rendering height in pixels */
   uint32_t bytes_per_scanline;     /* Glyphs scanline size in bytes */
} font_t;

typedef enum {
   ALIGN_LEFT,
   ALIGN_CENTER,
   ALIGN_RIGHT
} h_align_t;

/*
 * fb.c
 */
EXTERN font_t fb_font;

static INLINE unsigned int font_width(unsigned int len)
{
   return len * fb_font.width;
}

static INLINE unsigned int font_height(unsigned int rows)
{
   return rows * fb_font.height;
}

static INLINE size_t font_size(void)
{
   return 256 * fb_font.height * fb_font.bytes_per_scanline;
}

EXTERN void fb_clear(framebuffer_t *fb);
EXTERN int fb_init(vbe_mode_t *vbe, uintptr_t fb_addr, framebuffer_t *fb);
EXTERN void fb_draw_char(framebuffer_t *fb, const font_t *font, int c, int x,
                         int y, uint32_t rgba);
EXTERN void fb_print(framebuffer_t *fb, const char *str, int x, int y,
                     unsigned int width, uint32_t bg_rgba, uint32_t fg_rgba,
                     h_align_t align);
EXTERN void fb_draw_rect(framebuffer_t *fb, int x, int y, unsigned int width,
                         unsigned int height, uint32_t rgba);
EXTERN void fb_scroll_up(framebuffer_t *fb, unsigned int nlines);

#endif /* !FB_H_ */
