/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * uga.c -- VBE emulation for the UGA Draw protocol
 */

#include <bootlib.h>
#include "efi_private.h"

#define UGA_RED_MASK   0x00ff0000U
#define UGA_GREEN_MASK 0x0000ff00U
#define UGA_BLUE_MASK  0x000000ffU
#define UGA_ALPHA_MASK 0xff000000U

#define UGA_DEFAULT_REFRESH_RATE    60

static EFI_UGA_DRAW_PROTOCOL *uga = NULL;
static APPLE_BOOT_VIDEO_PROTOCOL *apple = NULL;

/*-- uga_set_video_mode --------------------------------------------------------
 *
 *      Switch to the given video mode using the UGA Draw protocol.
 *
 * Parameters
 *      IN width:  horizontal resolution (in pixels)
 *      IN height: vertical resolution (in pixels)
 *      IN depth:  number of bits per pixel (must be 32)
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS uga_set_video_mode(unsigned int width, unsigned int height,
                              unsigned int depth)
{
   if (uga == NULL || depth != 32) {
      return EFI_UNSUPPORTED;
   }

   /*
    * The Apple Xserve2,1 will crash if you call the SetMode function. An easy
    * workaround is to skip this call when running on an Apple machine.
    */
   if (apple != NULL) {
      return EFI_SUCCESS;
   }

   EFI_ASSERT_FIRMWARE(uga->SetMode != NULL);

   return uga->SetMode(uga, width, height, 32, UGA_DEFAULT_REFRESH_RATE);
}

/*-- uga_list_resolutions ------------------------------------------------------
 *
 *      List supported resolutions.
 *
 * Parameters
 *      OUT resolutions: pointer to the freshly allocated resolution list
 *      OUT count:       the number of supported resolutions
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS uga_list_resolutions(resolution_t **resolutions,
                                       unsigned int *count)
{
   resolution_t *res;
   UINT32 width, height, depth, refresh;
   EFI_STATUS Status;

   if (uga == NULL) {
      return EFI_UNSUPPORTED;
   }

   EFI_ASSERT_FIRMWARE(uga->GetMode != NULL);

   Status = uga->GetMode(uga, &width, &height, &depth, &refresh);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   res = sys_malloc(sizeof (resolution_t));
   if (res == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   res->width = (uint16_t)width;
   res->height = (uint16_t)height;
   res->depth = (uint8_t)depth;

   *resolutions = res;
   *count = 1;

   return EFI_SUCCESS;
}

/*-- uga_get_fb_info -----------------------------------------------------------
 *
 *      Return the UGA framebuffer properties for the current video mode.
 *
 * Parameters
 *      IN res: pointer to the input resolution structure
 *      IN fb:  pointer to the output framebuffer info structure
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS uga_get_fb_info(resolution_t *res, framebuffer_t *fb)
{
   UINT32 Addr, Size, BytesPerRow, Width, Height, Depth;
   EFI_STATUS Status;

   if (apple == NULL) {
      return EFI_UNSUPPORTED;
   }

   EFI_ASSERT_FIRMWARE(apple->GetFramebuffer != NULL);

   Status = apple->GetFramebuffer(apple, &Addr, &Size, &BytesPerRow, &Width,
                                  &Height, &Depth);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   /* The Macmini2,1 returns a totally bogus height. Fix it. */
   if (Height == 0) {
      Height = Size / BytesPerRow;
      efi_log(LOG_WARNING, "Unable to retrieve display height, guessing %d\n",
              Height);
   }

   if (res != NULL) {
      /* Only support probing fb info for the current graphic mode. */
      if (res->width != Width || res->height != Height || res->depth != Depth) {
         return EFI_UNSUPPORTED;
      }
   }

   fb->addr = UINT32_TO_PTR(Addr);
   fb->size = Size;
   fb->BytesPerScanLine = BytesPerRow;
   fb->width = Width;
   fb->height = Height;
   fb->depth = Depth;

   set_pixel_format(&fb->pxl, UGA_RED_MASK, UGA_GREEN_MASK, UGA_BLUE_MASK,
                    UGA_ALPHA_MASK);

   return EFI_SUCCESS;
}

/*- uga_init -------------------------------------------------------------------
 *
 *      Initialize the UGA protocol.
 *
 * Parameters
 *      OUT res:   a freshly allocated list of supported resolutions
 *      OUT count: the number of supported resolutions
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS uga_init(resolution_t **res, unsigned int *count)
{
   EFI_GUID AppleBootVideoProto = APPLE_BOOT_VIDEO_PROTOCOL_GUID;
   EFI_GUID UgaDrawProto = EFI_UGA_DRAW_PROTOCOL_GUID;
   EFI_STATUS Status;

   Status = LocateProtocol(&UgaDrawProto, (void **)&uga);
   if (EFI_ERROR(Status)) {
      uga = NULL;
      return Status;
   }

   Status = LocateProtocol(&AppleBootVideoProto, (void **)&apple);
   if (EFI_ERROR(Status)) {
      apple = NULL;
      return Status;
   }

   efi_log(LOG_DEBUG, "Apple UGA framebuffer detected\n");

   return uga_list_resolutions(res, count);
}
