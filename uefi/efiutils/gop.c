/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * gop.c -- VBE emulation for the Graphical Output Protocol
 */

#include <fb.h>
#include <bootlib.h>
#include "efi_private.h"

#define RGBA_RED_MASK   0x000000ffU
#define RGBA_GREEN_MASK 0x0000ff00U
#define RGBA_BLUE_MASK  0x00ff0000U
#define RGBA_ALPHA_MASK 0xff000000U

#define BGRA_RED_MASK   0x00ff0000U
#define BGRA_GREEN_MASK 0x0000ff00U
#define BGRA_BLUE_MASK  0x000000ffU
#define BGRA_ALPHA_MASK 0xff000000U

static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

/*-- gop_query_mode ------------------------------------------------------------
 *
 *      Wrapper to the gop->QueryMode() UEFI function that adds more error
 *      checking.
 *
 * Parameters
 *      IN  id:   mode number
 *      OUT mode: pointer to the freshly allocated GOP mode info structure
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS gop_query_mode(UINT32 id,
                                 EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **mode)
{
   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *gop_mode;
   UINTN size;
   EFI_STATUS Status;

   EFI_ASSERT_PARAM(mode != NULL);
   EFI_ASSERT(gop != NULL);
   EFI_ASSERT_FIRMWARE(gop->QueryMode != NULL);

   Status = gop->QueryMode(gop, id, &size, &gop_mode);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   if (size == 0 || mode == NULL) {
      return EFI_UNSUPPORTED;
   }

   *mode = gop_mode;

   return EFI_SUCCESS;
}

/*-- gop_get_pixel_layout ------------------------------------------------------
 *
 *      Get the pixel components information.
 *
 * Parameters
 *      IN pxl:         pointer pixel information output structure
 *      IN PixelFormat: the GOP pixel format
 *      IN PixelInfo:   the GOP pixel layout, if not specified by PixelFormat
 *
 * Result
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS gop_get_pixel_layout(pixel32_t *pxl,
                                       EFI_GRAPHICS_PIXEL_FORMAT PixelFormat,
                                       EFI_PIXEL_BITMASK *PixelInfo)
{
   switch (PixelFormat) {
      case PixelRedGreenBlueReserved8BitPerColor:
         set_pixel_format(pxl, RGBA_RED_MASK, RGBA_GREEN_MASK, RGBA_BLUE_MASK,
                          RGBA_ALPHA_MASK);
         break;
      case PixelBlueGreenRedReserved8BitPerColor:
         set_pixel_format(pxl, BGRA_RED_MASK, BGRA_GREEN_MASK, BGRA_BLUE_MASK,
                          BGRA_ALPHA_MASK);
         break;
      case PixelBitMask:
         set_pixel_format(pxl, PixelInfo->RedMask, PixelInfo->GreenMask,
                          PixelInfo->BlueMask, PixelInfo->ReservedMask);
         break;
      case PixelBltOnly:
         /* Direct framebuffer access not supported. */
         return EFI_UNSUPPORTED;
      default:
         /* Invalid pixel format */
         return EFI_INVALID_PARAMETER;
   }

   return EFI_SUCCESS;
}

/*-- gop_set_video_mode --------------------------------------------------------
 *
 *      Switch to the given video mode using the GOP protocol.
 *
 * Parameters
 *      IN width:  horizontal resolution (in pixels)
 *      IN height: vertical resolution (in pixels)
 *      IN depth:  number of bytes per pixel
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS gop_set_video_mode(unsigned int width, unsigned int height,
                              unsigned int depth)
{
   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode;
   pixel32_t pxl;
   EFI_STATUS Status;
   UINT32 i;

   for (i = 0; i < gop->Mode->MaxMode; i++) {
      Status = gop_query_mode(i, &mode);
      if (EFI_ERROR(Status)) {
         continue;
      }

      Status = gop_get_pixel_layout(&pxl, mode->PixelFormat,
                                    &mode->PixelInformation);
      if (!EFI_ERROR(Status)) {
         if (mode->HorizontalResolution == width &&
             mode->VerticalResolution == height &&
             (unsigned int)VBE_BPP(&pxl) == depth) {
            sys_free(mode);

            return gop->SetMode(gop, i);
         }
      }

      sys_free(mode);
   }

   return EFI_UNSUPPORTED;
}

/*-- gop_list_resolutions ------------------------------------------------------
 *
 *      List all supported resolutions.
 *
 * Parameters
 *      OUT resolutions: pointer to the freshly allocated resolution list
 *      OUT count:       number of supported resolutions
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS gop_list_resolutions(resolution_t **resolutions,
                                       unsigned int *count)
{
   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode;
   resolution_t *res;
   unsigned int n;
   UINT32 i;
   pixel32_t pxl;
   EFI_STATUS Status;

   res = sys_malloc(gop->Mode->MaxMode * sizeof (resolution_t));
   if (res == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   n = 0;

   for (i = 0; i < gop->Mode->MaxMode; i++) {
      Status = gop_query_mode(i, &mode);
      if (EFI_ERROR(Status)) {
         continue;
      }

      Status = gop_get_pixel_layout(&pxl, mode->PixelFormat,
                                    &mode->PixelInformation);

      if (!EFI_ERROR(Status) && IS_VBE_PIXEL(&pxl)) {
         res[n].width = (uint16_t)mode->HorizontalResolution;
         res[n].height = (uint16_t)mode->VerticalResolution;
         res[n].depth = (uint8_t)VBE_BPP(&pxl);
         n++;
      }

      sys_free(mode);
   }

   *count = n;
   *resolutions = res;

   return EFI_SUCCESS;
}

/*-- gop_get_fb_info -----------------------------------------------------------
 *
 *      Return the GOP framebuffer properties for the given display resolution.
 *      Passing a NULL resolution makes gop_get_fb_info() return the framebuffer
 *      information for the current resolution settings.
 *
 *      Note: The Xserve3,1 is known to have broken firmware where the
 *      QueryMode function reports an incorrect PixelsPerScanLine.
 *      gop->Mode->Info reports the correct value, and is used whenever
 *      possible.
 *
 * Parameters
 *      IN res: pointer to input resolution structure
 *      IN fb:  pointer to the output framebuffer info structure
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS gop_get_fb_info(resolution_t *res, framebuffer_t *fb)
{
   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *gop_mode;
   unsigned int i;
   UINT32 horizontalResolution;
   UINT32 verticalResolution;
   EFI_GRAPHICS_PIXEL_FORMAT pixelFormat;
   EFI_PIXEL_BITMASK pixelInformation;
   UINT32 pixelsPerScanLine;
   EFI_STATUS Status;

   fb->addr = UINT64_TO_PTR(gop->Mode->FrameBufferBase);
   fb->size = gop->Mode->FrameBufferSize;

   if (res) {
      for (i = 0; i < gop->Mode->MaxMode; i++) {
         if (i != gop->Mode->Mode) {
            Status = gop_query_mode(i, &gop_mode);
            if (EFI_ERROR(Status)) {
               continue;
            }
         } else {
            gop_mode = gop->Mode->Info;
         }

         horizontalResolution = gop_mode->HorizontalResolution;
         verticalResolution = gop_mode->VerticalResolution;
         pixelFormat = gop_mode->PixelFormat;
         pixelInformation = gop_mode->PixelInformation;
         pixelsPerScanLine = gop_mode->PixelsPerScanLine;

         if (i != gop->Mode->Mode) {
            sys_free(gop_mode);
         }

         Status = gop_get_pixel_layout(&fb->pxl, pixelFormat,
                                       &pixelInformation);
         if (EFI_ERROR(Status)) {
            continue;
         }

         if (res->width == horizontalResolution &&
             res->height == verticalResolution &&
             res->depth == VBE_BPP(&fb->pxl)) {
            fb->width = horizontalResolution;
            fb->height = verticalResolution;
            fb->depth = VBE_BPP(&fb->pxl);
            fb->BytesPerScanLine = (pixelsPerScanLine * VBE_BPP(&fb->pxl)) / 8;

            return EFI_SUCCESS;
         }
      }
      /* Direct FB access not supported for the requested resolution. */
      return EFI_UNSUPPORTED;
   } else {
      gop_mode = gop->Mode->Info;

      Status = gop_get_pixel_layout(&fb->pxl, gop_mode->PixelFormat,
                                    &gop_mode->PixelInformation);
      if (EFI_ERROR(Status)) {
         /* Direct FB access not supported for the current resolution. */
         return Status;
      }

      fb->width = gop_mode->HorizontalResolution;
      fb->height = gop_mode->VerticalResolution;
      fb->depth = VBE_BPP(&fb->pxl);
      fb->BytesPerScanLine = (gop_mode->PixelsPerScanLine *
                              VBE_BPP(&fb->pxl)) / 8;
   }

   return EFI_SUCCESS;
}

/*-- gop_init ------------------------------------------------------------------
 *
 *      Initialize the GOP protocol.
 *
 * Parameters
 *      OUT res:   pointer to the freshly allocated list of supported resolutions
 *      OUT count: number of supported resolutions
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS gop_init(resolution_t **res, unsigned int *count)
{
   EFI_GUID gop_protocol = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
   EFI_STATUS Status;

   Status = LocateProtocol(&gop_protocol, (void **)&gop);
   if (EFI_ERROR(Status)) {
      gop = NULL;
      return Status;
   }

   efi_log(LOG_DEBUG, "GOP framebuffer @ 0x%llx (%zu bytes)\n",
           gop->Mode->FrameBufferBase, gop->Mode->FrameBufferSize);

   if (gop->Mode->MaxMode == 0) {
      /* GOP protocol is present, but no video mode is available */
      return EFI_UNSUPPORTED;
   }

   return gop_list_resolutions(res, count);
}
