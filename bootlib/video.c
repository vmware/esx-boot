/*******************************************************************************
 * Copyright (c) 2008-2011,2014,2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * video.c -- High level video modes management
 */

#include <bootlib.h>
#include <boot_services.h>
#include <vbe.h>
#include <fb.h>
#include <string.h>
#include <limits.h>

static vbe_info_t vbe;
static bool video_is_initialized = false;

static void vbe_mode_dump(vbe_mode_id_t id, vbe_mode_t *mode, uintptr_t fb_addr)
{
   Log(LOG_DEBUG, "VBE: mode=0x%x %ux%ux%u @ %p, "
       "atrr=0x%x model=%u DirectColor=%u\n", id,
       (uint32_t)mode->XResolution, (uint32_t)mode->YResolution,
       (uint32_t)mode->BitsPerPixel, UINT_TO_PTR(fb_addr),
       (uint32_t)mode->ModeAttributes, (uint32_t)mode->MemoryModel,
       (uint32_t)mode->DirectColorModeInfo);

   Log(LOG_DEBUG, "VBE: Windows (%u bytes) granularity=%u-Kb func=0x%x\n",
       (uint32_t)mode->WinSize, (uint32_t)mode->WinGranularity,
       (uint32_t)mode->WinFuncPtr);
   Log(LOG_DEBUG, "VBE: WinA (seg=%x attr=%x), WinB (seg=%x attr=%x)\n",
       (uint32_t)mode->WinASegment, (uint32_t)mode->WinAAttributes,
       (uint32_t)mode->WinBSegment, (uint32_t)mode->WinABttributes);

   Log(LOG_DEBUG, "VBE: Font=%ux%u planes=%u banks=%u (%u-Kb each), "
       "ipp=%u maxpxlclock=%u\n",
       (uint32_t)mode->XCharSize, (uint32_t)mode->YCharSize,
       (uint32_t)mode->NumberOfPlanes, (uint32_t)mode->NumberOfBanks,
       (uint32_t)mode->BankSize, (uint32_t)mode->NumberOfImagePages,
       (uint32_t)mode->MaxPixelClock);

   Log(LOG_DEBUG, "VBE: ARGB %u:%u:%u:%u (%u:%u:%u:%u), "
       "scanline=%u bankIPP=%u\n",
       (uint32_t)mode->RsvdMaskSize, (uint32_t)mode->RedMaskSize,
       (uint32_t)mode->GreenMaskSize, (uint32_t)mode->BlueMaskSize,
       (uint32_t)mode->RsvdFieldPosition, (uint32_t)mode->RedFieldPosition,
       (uint32_t)mode->GreenFieldPosition, (uint32_t)mode->BlueFieldPosition,
       (uint32_t)mode->BytesPerScanLine, (uint32_t)mode->BnkNumberOfImagePages);

   Log(LOG_DEBUG, "VBE: ARGB %u:%u:%u:%u (%u:%u:%u:%u), "
       "scanline=%u LinIPP=%u (Lin)\n",
       (uint32_t)mode->LinRsvdMaskSize, (uint32_t)mode->LinRedMaskSize,
       (uint32_t)mode->LinGreenMaskSize, (uint32_t)mode->LinBlueMaskSize,
       (uint32_t)mode->LinRsvdFieldPosition, (uint32_t)mode->LinRedFieldPosition,
       (uint32_t)mode->LinGreenFieldPosition, (uint32_t)mode->LinBlueFieldPosition,
       (uint32_t)mode->LinBytesPerScanLine, (uint32_t)mode->LinNumberOfImagePages);
}

/*-- video_init ----------------------------------------------------------------
 *
 *      Initialize the graphical display.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int video_init(void)
{
   int status;

   if (video_is_initialized) {
      return ERR_ALREADY_STARTED;
   }

   status = set_display_mode(DISPLAY_MODE_VBE);
   if (status != ERR_SUCCESS) {
      return status;
   }

   memset(&vbe, 0, sizeof (vbe_info_t));

   status = vbe_get_info(&vbe.controller, &vbe.modes_list);
   if (status != ERR_SUCCESS) {
      return status;
   }

   Log(LOG_DEBUG, "VBE: version %u.%u %u Kb of memory\n",
       (uint32_t)(vbe.controller.VbeVersion >> 8),
       (uint32_t)(vbe.controller.VbeVersion & 0xff),
       (uint32_t)(vbe.controller.TotalMemory * 64));

   video_is_initialized = true;

   return ERR_SUCCESS;
}

/*-- video_check_support -------------------------------------------------------
 *
 *      Check whether the VBE interface is supported, properly initialized, and
 *      that at least one VBE mode is available.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int video_check_support(void)
{
   if (!video_is_initialized) {
      return ERR_NOT_STARTED;
   }

   if (vbe.modes_list == NULL) {
      return ERR_UNSUPPORTED;
   }

   return ERR_SUCCESS;
}

/*-- video_scan_modes ----------------------------------------------------------
 *
 *      Scan the supported video modes and return the one that matches the best.
 *
 * Parameters
 *      IN  width:         preferred horizontal resolution, in pixels
 *      IN  height:        preferred vertical resolution, in pixels
 *      IN  depth:         preferred color depth, in bits per pixel
 *      IN  min_width:     minimum horizontal resolution, in pixels
 *      IN  min_height:    minimum vertical resolution, in pixels
 *      IN  min_depth:     minimum color depth, in bits per pixel
 *      IN  debug:         if true, log all modes
 *      OUT best_mode_id:  VBE mode id
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int video_scan_modes(unsigned int width, unsigned int height,
                            unsigned int depth,
                            unsigned int min_width, unsigned int min_height,
                            unsigned int min_depth, bool debug,
                            vbe_mode_id_t *best_mode_id)
{
   uint32_t pix_delta, best_pix_delta, prefered_pixels_number;
   vbe_mode_id_t best, mode_id;
   uint16_t attributes;
   vbe_mode_t mode;
   uintptr_t mode_fb_addr;
   int i;

   best = VBE_MODE_INVAL;
   best_pix_delta = UINT_MAX;
   prefered_pixels_number = width * height;

   attributes = VBE_MODE_ATTR_AVAILABLE | VBE_MODE_ATTR_GRAPHIC |
      VBE_MODE_ATTR_COLOR | VBE_MODE_ATTR_LINEAR;

   for (i = 0; vbe.modes_list[i] != VBE_MODE_INVAL; i++) {
      mode_id = vbe.modes_list[i];

      if (vbe_get_mode_info(mode_id, &mode, &mode_fb_addr) != ERR_SUCCESS) {
         continue;
      }

      if (debug) {
         vbe_mode_dump(mode_id, &mode, mode_fb_addr);
      }

      if (((mode.ModeAttributes & attributes) != attributes) ||
          (mode.MemoryModel != VBE_MEMORY_MODEL_PACKED_PIXEL &&
           mode.MemoryModel != VBE_MEMORY_MODEL_DIRECT_COLOR)) {
         continue;
      }

      if (mode.XResolution < min_width || mode.YResolution < min_height ||
          mode.BitsPerPixel < min_depth) {
         continue;
      }

      if (width == mode.XResolution && height == mode.YResolution &&
          depth == mode.BitsPerPixel) {
         best = mode_id;
         if (debug) {
            Log(LOG_DEBUG, "Found exact match for video mode, id 0x%x", best);
         } else {
            break;
         }
      }

      pix_delta = mode.XResolution * mode.YResolution;
      if (pix_delta > prefered_pixels_number) {
         pix_delta -= prefered_pixels_number;
      } else {
         pix_delta = prefered_pixels_number - pix_delta;
      }

      if (mode.XResolution < width || mode.YResolution < height) {
         pix_delta *= 2;
      }

      if (pix_delta <= best_pix_delta) {
         best = mode_id;
         best_pix_delta = pix_delta;
      }
   }

   if (best == VBE_MODE_INVAL) {
      return ERR_NOT_FOUND;
   }

   *best_mode_id = best;

   return ERR_SUCCESS;
}

/*-- video_set_text_mode -------------------------------------------------------
 *
 *      Toggle display to VGA text mode.
 *      This function first initializes the video subsystem if required.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int video_set_text_mode(void)
{
   int status;

   status = video_check_support();
   if (status == ERR_NOT_STARTED) {
      status = video_init();
   }

   if (status != ERR_SUCCESS) {
      return status;
   }

   return vbe_force_vga_text(&vbe.current_mode, &vbe.mode);
}

/*-- video_set_mode ------------------------------------------------------------
 *
 *      Set the video to the specified resolution.
 *      This function first initializes the video subsystem if required.
 *
 * Parameters
 *      IN fb:            pointer to where to store the new framebuffer info
 *      IN width:         preferred horizontal resolution, in pixels
 *      IN height:        preferred vertical resolution, in pixels
 *      IN depth:         preferred color depth, in bits per pixel
 *      IN min_width:     minimum horizontal resolution, in pixels
 *      IN min_height:    minimum vertical resolution, in pixels
 *      IN min_depth:     minimum color depth, in bits per pixel
 *      IN debug:         if true, log all modes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int video_set_mode(framebuffer_t *fb, unsigned int width, unsigned int height,
                   unsigned int depth, unsigned int min_width,
                   unsigned int min_height, unsigned int min_depth, bool debug)
{
   vbe_mode_id_t id;
   int status;

   status = video_check_support();
   if (status == ERR_NOT_STARTED) {
      status = video_init();
   }

   if (status != ERR_SUCCESS) {
      return status;
   }

   status = video_scan_modes(width, height, depth,
                             min_width, min_height, min_depth,
                             debug, &id);
   if (status != ERR_SUCCESS) {
      return status;
   }

   /*
    * With legacy BIOS, firmware_print is unsafe after vbe_set_mode.
    */
   log_unsubscribe(firmware_print);

   status = vbe_set_mode(id);
   if (status != ERR_SUCCESS) {
      return status;
   }

   /*
    * We get the VBE mode info (including framebuffer base and size)
    * here *after* setting mode, because the size and base can
    * differ for different resolutions.
    */
   status = vbe_get_mode_info(id, &vbe.mode, &vbe.fb_addr);
   if (status != ERR_SUCCESS) {
      return status;
   }

   vbe.current_mode = id;
   vbe_mode_dump(id, &vbe.mode, vbe.fb_addr);

   return fb_init(&vbe.mode, vbe.fb_addr, fb);
}

/*-- video_get_vbe_info --------------------------------------------------------
 *
 *      Get the VBE information for the current video mode.
 *
 * Parameters
 *      IN vbe_info: pointer to where to store the VBE info
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int video_get_vbe_info(vbe_info_t *vbe_info)
{
   int status;

   status = video_check_support();
   if (status != ERR_SUCCESS) {
      return status;
   }

   memcpy(vbe_info, &vbe, sizeof (vbe_info_t));

   return ERR_SUCCESS;
}
