/*******************************************************************************
 * Copyright (c) 2008-2011,2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * vbe.c -- VBE implementation for EFI
 */

#include <string.h>
#include <vbe.h>
#include "efi_private.h"

typedef struct {
   resolution_t res;
   vbe_mode_id_t id;
} vbe_resolution_t;

typedef struct {
   EFI_STATUS (*get_fb_info)(resolution_t *res, framebuffer_t *fb);
   EFI_STATUS (*set_video_mode)(unsigned int w, unsigned int h,
                                unsigned int bpp);
} efi_vbe_operations_t;

typedef enum {
   VBE_COLOR_16      = 4,
   VBE_COLOR_256     = 8,
   VBE_COLOR_15_BITS = 15,
   VBE_COLOR_16_BITS = 16,
   VBE_COLOR_24_BITS = 24,
   VBE_COLOR_32_BITS = 32
} vbe_color_t;

#define VESA_MODES_NR    23

#define FOREACH_VBE_MODE(_vbe_id_, _res_)                         \
   for ((_vbe_id_) = vbe_modes[0], (_res_) = resolutions;         \
        (_vbe_id_) != VBE_MODE_INVAL;                             \
        (_vbe_id_) = vbe_modes[++(_res_) - resolutions])

static efi_vbe_operations_t video_ops;
static resolution_t *resolutions = NULL;  /* Supported resolutions */
static vbe_mode_id_t *vbe_modes = NULL;   /* Supported VBE mode numbers */

/* Standard VESA mode numbers (as defined in the VBE3.0 specification) */
static const vbe_resolution_t vesa_modes[VESA_MODES_NR] = {
   { { 640,  400, VBE_COLOR_256},     0x100 },
   { { 640,  480, VBE_COLOR_256},     0x101 },
   { { 800,  600, VBE_COLOR_16},      0x102 },
   { { 800,  600, VBE_COLOR_256},     0x103 },
   { {1024,  768, VBE_COLOR_16},      0x104 },
   { {1024,  768, VBE_COLOR_256},     0x105 },
   { {1280, 1024, VBE_COLOR_16},      0x106 },
   { {1280, 1024, VBE_COLOR_256},     0x107 },
   { { 320,  200, VBE_COLOR_15_BITS}, 0x10d },
   { { 320,  200, VBE_COLOR_16_BITS}, 0x10e },
   { { 320,  200, VBE_COLOR_24_BITS}, 0x10f },
   { { 640,  480, VBE_COLOR_15_BITS}, 0x110 },
   { { 640,  480, VBE_COLOR_16_BITS}, 0x111 },
   { { 640,  480, VBE_COLOR_24_BITS}, 0x112 },
   { { 800,  600, VBE_COLOR_15_BITS}, 0x113 },
   { { 800,  600, VBE_COLOR_16_BITS}, 0x114 },
   { { 800,  600, VBE_COLOR_24_BITS}, 0x115 },
   { {1024,  768, VBE_COLOR_15_BITS}, 0x116 },
   { {1024,  768, VBE_COLOR_16_BITS}, 0x117 },
   { {1024,  768, VBE_COLOR_24_BITS}, 0x118 },
   { {1280, 1024, VBE_COLOR_15_BITS}, 0x119 },
   { {1280, 1024, VBE_COLOR_16_BITS}, 0x11a },
   { {1280, 1024, VBE_COLOR_24_BITS}, 0x11b }
};

/*-- vbe_list_mode_numbers -----------------------------------------------------
 *
 *      List VBE mode numbers (given supported resolutions).
 *
 * Parameters
 *      IN  res:   the list of supported resolutions
 *      IN  count: the number of supported resolutions
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS vbe_list_mode_numbers(resolution_t *res, unsigned int count)
{
   vbe_mode_id_t efi_mode_id;
   unsigned int i, j;

   vbe_modes = sys_malloc((count + 1) * sizeof (vbe_mode_id_t));
   if (vbe_modes == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   efi_mode_id = vesa_modes[VESA_MODES_NR - 1].id + 1;

   for (i = 0; i < count; i++) {
      vbe_modes[i] = VBE_MODE_INVAL;

      for (j = 0; j < VESA_MODES_NR; j++) {
          if (res[i].width == vesa_modes[j].res.width &&
              res[i].height == vesa_modes[j].res.height &&
              res[i].depth == vesa_modes[j].res.depth) {
             vbe_modes[i] = vesa_modes[j].id;
             break;
          }
       }

      if (vbe_modes[i] == VBE_MODE_INVAL) {
         /* Not a VBE standard resolution */
         vbe_modes[i] = efi_mode_id++;
      }
   }

   /* VBE mode list must be terminated by a -1 (0FFFFh) */
   vbe_modes[i] = VBE_MODE_INVAL;

   return EFI_SUCCESS;
}

/*-- efi_init_vbe --------------------------------------------------------------
 *
 *      Initialize the EFI VBE emulation.
 *      When present, GOP Protocol is used by default. Systems that do not
 *      provide GOP are initialized through the UGA Protocol.
 *
 * Parameters
 *      IN  vbe:   pointer to the VBE controller information output structure
 *      OUT modes: pointer to a freshly allocated list of supported modes ID's
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS efi_init_vbe(void)
{
   unsigned int n;
   EFI_STATUS Status;

   Status = gop_init(&resolutions, &n);
   if (!EFI_ERROR(Status)) {
      video_ops.get_fb_info = gop_get_fb_info;
      video_ops.set_video_mode = gop_set_video_mode;
   } else {
      Status = uga_init(&resolutions, &n);
      if (!EFI_ERROR(Status)) {
         video_ops.get_fb_info = uga_get_fb_info;
         video_ops.set_video_mode = uga_set_video_mode;
      } else {
         return EFI_UNSUPPORTED;
      }
   }

   Status = vbe_list_mode_numbers(resolutions, n);
   if (EFI_ERROR(Status)) {
      efi_clean_vbe();
      return Status;
   }

   return EFI_SUCCESS;
}

/*-- get_mask32_info -----------------------------------------------------------
 *
 *      Convert a 32-bit color component mask to VBE component format.
 *
 * Parameters
 *      IN bitmask: the 32-bit mask
 *      OUT size:   the component size, in bits
 *      OUT offset: the component offset, in bits, starting from LSB
 *----------------------------------------------------------------------------*/
static void get_mask32_info(uint32_t bitmask, uint8_t *size, uint8_t *offset)
{
   uint8_t sz, offt;

   sz = 0;
   offt = 0;

   if (bitmask != 0) {
      while (!(bitmask & 1)) {
         offt++;
         bitmask >>= 1;
      }
      while (bitmask & 1) {
         sz++;
         bitmask >>= 1;
      };
   }

   *size = sz;
   *offset = offt;
}

/*-- set_pixel_format ----------------------------------------------------------
 *
 *      Set the format of a generic pixel.
 *
 * Parameters
 *      IN pxl:      pointer to the pixel info structure
 *      IN red:      the 8-bits red component value
 *      IN green:    the 8-bits green component value
 *      IN blue:     the 8-bits blue component value
 *      IN reserved: the 8-bits alpha/reserved component value
 *----------------------------------------------------------------------------*/
void set_pixel_format(pixel32_t *pxl, uint32_t red, uint32_t green,
                      uint32_t blue, uint32_t reserved)
{
   EFI_ASSERT_PARAM(pxl != NULL);

   get_mask32_info(red, &pxl->RedSize, &pxl->RedOffset);
   get_mask32_info(green, &pxl->GreenSize, &pxl->GreenOffset);
   get_mask32_info(blue, &pxl->BlueSize, &pxl->BlueOffset);
   get_mask32_info(reserved, &pxl->RsvdSize, &pxl->RsvdOffset);
}

/*-- vbe_get_mode_info ---------------------------------------------------------
 *
 *      Return VBE Mode Information. (VBE Specification, function 01h)
 *
 * Parameters
 *      IN id:       the VBE mode id
 *      IN mode:     pointer to the output mode info structure
 *      OUT fb_addr: framebuffer addres
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_get_mode_info(vbe_mode_id_t id, vbe_mode_t *mode, uintptr_t *fb_addr)
{
   vbe_mode_id_t vbe_id;
   resolution_t *res;
   framebuffer_t fb;
   EFI_STATUS Status;

   FOREACH_VBE_MODE(vbe_id, res) {
      if (id == vbe_id) {
         break;
      }
   }

   if (vbe_id == VBE_MODE_INVAL) {
      return error_efi_to_generic(EFI_INVALID_PARAMETER);
   }

   Status = video_ops.get_fb_info(res, &fb);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   memset(mode, 0, sizeof (vbe_mode_t));
   mode->ModeAttributes = VBE_MODE_ATTR_AVAILABLE
      | VBE_MODE_ATTR_VBE12_EXTENSION
      | VBE_MODE_ATTR_GRAPHIC
      | VBE_MODE_ATTR_LINEAR
      | VBE_MODE_ATTR_NO_WINDOW
      | VBE_MODE_ATTR_NON_VGA
      | VBE_MODE_ATTR_COLOR;
   mode->BytesPerScanLine = (uint16_t)fb.BytesPerScanLine;
   mode->XResolution = (uint16_t)fb.width;
   mode->YResolution = (uint16_t)fb.height;
   mode->NumberOfPlanes = 1;     /* Not a planar mode */
   mode->BitsPerPixel = VBE_BPP(&fb.pxl);
   mode->NumberOfBanks = 1;     /* 1 for modes that do not have banks. */
   mode->MemoryModel = VBE_MEMORY_MODEL_DIRECT_COLOR;
   mode->NumberOfImagePages = 1;
   mode->Reserved0 = 1;         /* VBE <=3.0: must be set to 1 */
   mode->RedMaskSize = fb.pxl.RedSize;
   mode->RedFieldPosition = fb.pxl.RedOffset;
   mode->GreenMaskSize = fb.pxl.GreenSize;
   mode->GreenFieldPosition = fb.pxl.GreenOffset;
   mode->BlueMaskSize = fb.pxl.BlueSize;
   mode->BlueFieldPosition = fb.pxl.BlueOffset;
   mode->RsvdMaskSize = fb.pxl.RsvdSize;
   mode->RsvdFieldPosition = fb.pxl.RsvdOffset;
   mode->PhysBasePtr = PTR_TO_UINT32(fb.addr);
   *fb_addr = PTR_TO_UINT(fb.addr);
   mode->LinBytesPerScanLine = (uint16_t)fb.BytesPerScanLine;
   mode->LinNumberOfImagePages = 1;
   mode->LinRedMaskSize = mode->RedMaskSize;
   mode->LinRedFieldPosition = mode->RedFieldPosition;
   mode->LinGreenMaskSize = mode->GreenMaskSize;
   mode->LinGreenFieldPosition = mode->GreenFieldPosition;
   mode->LinBlueMaskSize = mode->BlueMaskSize;
   mode->LinBlueFieldPosition = mode->BlueFieldPosition;
   mode->LinRsvdMaskSize = mode->RsvdMaskSize;
   mode->LinRsvdFieldPosition = mode->RsvdFieldPosition;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- vbe_get_info --------------------------------------------------------------
 *
 *      Return VBE controller information. (VBE Specification, Function 00h)
 *
 * Parameters
 *      IN  vbe:   pointer to the output VBE controller info structure
 *      OUT modes: pointer to the freshly allocated list of supported modes ID's
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
int vbe_get_info(vbe_t *vbe, vbe_mode_id_t **modes)
{
   resolution_t *res;
   vbe_mode_id_t id;
   framebuffer_t fb;
   size_t size;
   EFI_STATUS Status;

   if (vbe_modes == NULL) {
      Status = efi_init_vbe();
      if (Status != EFI_SUCCESS) {
         return error_efi_to_generic(Status);
      }
   }

   *modes = NULL;

   Status = video_ops.get_fb_info(NULL, &fb);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (fb.size == 0) {
      /*
       * Framebuffer size is not provided by the underlying graphic protocol,
       * so we consider that it should be large enough to operate at the
       * highest supported resolution.
       */
      FOREACH_VBE_MODE(id, res) {
         size = (size_t)res->width * (size_t)res->height * (size_t)res->depth;
         if (size > fb.size) {
            fb.size = size;
         }
      }

      fb.size /= 8;
   }

   memset(vbe, 0, sizeof (vbe_t));
   vbe->VbeSignature = VESA_MAGIC;
   vbe->VbeVersion = VBE_VERSION;
   vbe->Capabilities = VBE_CAP_NO_VGA;
   vbe->TotalMemory = (uint16_t)(fb.size / (64 * 1024)); /* in 64Kb units */

   /*
    * vbe->VideoModePtr and vbe->OemStringPtr are too small (4-bytes each) to
    * hold 64-bit addresses. These fields must be filled up later when the
    * bootloader relocates the structures in the 32-bit address-space.
    */
   vbe->OemStringPtr = 0;
   vbe->VideoModePtr = 0;

   *modes = vbe_modes;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- vbe_set_mode --------------------------------------------------------------
 *
 *      Set VBE mode to the specified supported resolution.
 *
 * Parameters
 *      IN id: VBE mode identifier
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_set_mode(vbe_mode_id_t id)
{
   resolution_t *res;
   vbe_mode_id_t vbe_id;
   EFI_STATUS Status;
   int status;

   status = vbe_get_current_mode(&vbe_id);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (id == vbe_id) {
      return ERR_SUCCESS;
   }

   FOREACH_VBE_MODE(vbe_id, res) {
      if (id == vbe_id) {
         Status = video_ops.set_video_mode(res->width, res->height, res->depth);
         return error_efi_to_generic(Status);
      }
   }

   return error_efi_to_generic(EFI_UNSUPPORTED);
}

/*-- vbe_get_current_mode ------------------------------------------------------
 *
 *      Return current VBE mode (VBE Specification, Function 03h)
 *
 * Parameters
 *      OUT id: 16-bit id of the current VBE mode
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_get_current_mode(vbe_mode_id_t *id)
{
   resolution_t *res;
   vbe_mode_id_t vbe_id;
   framebuffer_t fb;
   EFI_STATUS Status;

   Status = video_ops.get_fb_info(NULL, &fb);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   FOREACH_VBE_MODE(vbe_id, res) {
      if (fb.width == res->width && fb.height == res->height &&
          fb.depth == res->depth) {
         *id = vbe_id | VBE_MODE_ID_ATTR_LINEAR;
         return error_efi_to_generic(EFI_SUCCESS);
      }
   }

   return error_efi_to_generic(EFI_NOT_FOUND);
}

/*-- efi_clean_vbe -------------------------------------------------------------
 *
 *      Clean the VBE interface.
 *----------------------------------------------------------------------------*/
void efi_clean_vbe(void)
{
   if (vbe_modes != NULL) {
      sys_free(vbe_modes);
      vbe_modes = NULL;
   }

   if (resolutions != NULL) {
      sys_free(resolutions);
      resolutions = NULL;
   }

   video_ops.get_fb_info = NULL;
   video_ops.set_video_mode = NULL;
}

/*-- vbe_force_vga_text --------------------------------------------------------
 *
 *      Not supported by UEFI.
 *
 * Parameters
 *      OUT id:   Unused
 *      OUT mode: Unused
 *
 * Results
 *      ERR_UNSUPPORTED
 *----------------------------------------------------------------------------*/
int vbe_force_vga_text(UNUSED_PARAM(vbe_mode_id_t *id),
                       UNUSED_PARAM(vbe_mode_t *mode))
{
   return ERR_UNSUPPORTED;
}
