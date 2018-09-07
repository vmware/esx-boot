/*******************************************************************************
 * Copyright (c) 2008-2011,2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * vbe.c -- VBE implementation for BIOS
 */

#include <string.h>
#include "com32_private.h"

#define COM32_VIDEO_GRAPHIC_MODE        (1 << 0)
#define COM32_VIDEO_NON_STANDARD        (1 << 1)
#define COM32_VIDEO_VESA_MODE           (1 << 2)
#define COM32_VIDEO_NO_TEXT             (1 << 3)

/*-- com32_force_text_mode -----------------------------------------------------
 *
 *      Wrapper to the 'Force text mode' COM32 service.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_force_text_mode(void)
{
   com32sys_t iregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x05;

   return intcall_check_CF(COM32_INT, &iregs, NULL);
}

/*-- com32_report_video_mode ---------------------------------------------------
 *
 *      Wrapper to the 'Report video mode change' COM32 service.
 *
 * Parameters
 *      IN flags:  Video mode flags
 *      IN width:  graphics modes, pixel columns
 *      IN height: graphics modes, pixel rows
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_report_video_mode(uint16_t flags, uint16_t width,
                                   uint16_t height)
{
   com32sys_t iregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x17;
   iregs.ebx.w[0] = flags;
   if ((flags & COM32_VIDEO_GRAPHIC_MODE) == COM32_VIDEO_GRAPHIC_MODE) {
      iregs.ecx.w[0] = width;
      iregs.edx.w[0] = height;
   }

   return intcall_check_CF(COM32_INT, &iregs, NULL);
}

/*-- int10_vbe -----------------------------------------------------------------
 *
 *      VBE BIOS interrupt 10h wrapper.
 *
 *      VBE error codes:
 *        AL == 4Fh: Function is supported
 *        AL != 4Fh: Function is not supported
 *        AH == 00h: Function call successful
 *        AH == 01h: Function call failed
 *        AH == 02h: Function is not supported in the current hardware config
 *        AH == 03h: Function call invalid in current video mode
 *
 * Parameters
 *      IN iregs: pointer to the input registers
 *      IN oregs: pointer to the output registers
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int int10_vbe(const com32sys_t *iregs, com32sys_t *oregs)
{
   com32sys_t tmpregs;

   if (oregs == NULL) {
      oregs = &tmpregs;
   }

   intcall(0x10, iregs, oregs);

   if (oregs->eax.b[0] != 0x4f) {
      return ERR_UNSUPPORTED;
   }

   switch (oregs->eax.b[1]) {
      case 0:
         return ERR_SUCCESS;
      case 1:
         return ERR_DEVICE_ERROR;
      case 2:
         return ERR_UNSUPPORTED;
      case 3:
         return ERR_INVALID_PARAMETER;
      default:
         return ERR_DEVICE_ERROR;
   }
}

/*-- int10_get_vbe_info --------------------------------------------------------
 *
 *      Return VBE controller information. (VBE Specification, Function 00h)
 *
 * Parameters
 *      IN vbe: pointer to where to copy the VBE controller info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int int10_get_vbe_info(vbe_t *vbe)
{
   com32sys_t iregs;
   farptr_t fptr;
   int status;
   vbe_t *buf;

   buf = get_bounce_buffer();
   memset(buf, 0, sizeof (vbe_t));
   buf->VbeSignature = VBE2_MAGIC;

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(buf);
   iregs.eax.w[0] = 0x4f00;
   iregs.edi.w[0] = fptr.real.offset;
   iregs.es = fptr.real.segment;

   status = int10_vbe(&iregs, NULL);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (buf->VbeSignature != VESA_MAGIC) {
      return ERR_DEVICE_ERROR;
   }

   memcpy(vbe, buf, sizeof (vbe_t));

   return ERR_SUCCESS;
}

/*-- int10_get_vbe_mode_info ---------------------------------------------------
 *
 *      Return VBE Mode Information. (VBE Specification, function 01h)
 *
 *      NOTE: On BIOS platforms, supported VBE modes are listed with the VBE
 *            function 00h which returns a list of supported VBE mode numbers.
 *            Each of these VBE mode numbers can be passed to the VBE function
 *            01h in order to get the mode information. When doing so, the VBE
 *            mode number may not be altered with any mode flag such as
 *            VBE_MODE_ID_ATTR_LINEAR. Most platforms ignore additional flags,
 *            but the SunFire X4440 don't.
 *
 *      NOTE: may return ERR_UNSUPPORTED if the video mode has not been set
 *            first with vbe_set_mode().
 *
 * Parameters
 *      IN id:   the VBE mode number
 *      IN mode: pointer to where to copy the mode info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int int10_get_vbe_mode_info(uint16_t id, vbe_mode_t *mode)
{
   com32sys_t iregs;
   vbe_mode_t *buf;
   farptr_t fptr;
   int status;

   buf = get_bounce_buffer();
   memset(buf, 0, sizeof (vbe_mode_t));

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(buf);
   iregs.eax.w[0] = 0x4f01;
   iregs.ecx.w[0] = id;
   iregs.edi.w[0] = fptr.real.offset;
   iregs.es = fptr.real.segment;

   status = int10_vbe(&iregs, NULL);
   if (status != ERR_SUCCESS) {
      return status;
   }

   memcpy(mode, buf, sizeof (vbe_mode_t));

   return ERR_SUCCESS;
}

/*-- int10_set_vbe_mode --------------------------------------------------------
 *
 *      Set VBE mode to the specified supported resolution.
 *
 * Parameters
 *      IN id: VBE mode id
 *      IN crtc: pointer to the CRTC info block structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int int10_set_vbe_mode(uint16_t id, const vbe_crtc_t *crtc)
{
   com32sys_t iregs;
   farptr_t fptr;
   void *buf;

  /* Reserved bits 9, 10, 12 and 13 must be cleared. */
   id &= ~((1 << 13) | (1 << 12) | (1 << 10) | (1 << 9));

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x4f02;
   iregs.ebx.w[0] = id;

   if ((id & (1 << 11)) != 0) {
      if (crtc == NULL) {
         return ERR_INVALID_PARAMETER;
      }

      buf = get_bounce_buffer();
      memcpy(buf, crtc, sizeof (vbe_crtc_t));

      fptr = virtual_to_real(buf);
      iregs.edi.w[0] = fptr.real.offset;
      iregs.es = fptr.real.segment;
   }

   return int10_vbe(&iregs, NULL);
}

/*-- int10_get_current_vbe_mode ------------------------------------------------
 *
 *      Return current VBE mode (VBE Specification, Function 03h)
 *
 *      NOTE: VBE 3.0 Specification says:
 *        "This function is not guaranteed to return an accurate mode value if
 *         the mode set was not done with VBE function 02h."
 *
 * Parameters
 *      OUT id: current VBE mode 16-bit id
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int int10_get_current_vbe_mode(uint16_t *id)
{
   com32sys_t iregs, oregs;
   int status;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x4f03;

   status = int10_vbe(&iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *id = oregs.ebx.w[0];

   return ERR_SUCCESS;
}

/*-- vbe_get_mode_info ---------------------------------------------------------
 *
 *      Return VBE Mode Information.
 *
 *      NOTE: may return ERR_UNSUPPORTED if the video mode has not been set
 *            first with vbe_set_mode().
 *
 * Parameters
 *      IN  id:      the VBE mode id
 *      IN  mode:    pointer to the output mode info structure
 *      OUT fb_addr: framebuffer address
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_get_mode_info(vbe_mode_id_t id, vbe_mode_t *mode, uintptr_t *fb_addr)
{
   int ret;

   ret = int10_get_vbe_mode_info(id, mode);
   if (ret == ERR_SUCCESS) {
      *fb_addr = mode->PhysBasePtr;
   }

   return ret;
}

/*-- vbe_get_info --------------------------------------------------------------
 *
 *      Return VBE controller information.
 *
 * Parameters
 *      IN  vbe:   pointer to the output VBE controller info structure
 *      OUT modes: pointer to the freshly allocated list of supported modes ID's
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_get_info(vbe_t *vbe, vbe_mode_id_t **modes)
{
   size_t count, modes_list_size;
   vbe_mode_id_t *m, *modes_list;
   farptr_t fptr;
   int status;

   status = int10_get_vbe_info(vbe);
   if (status != ERR_SUCCESS) {
      return status;
   }

   fptr.ptr = vbe->VideoModePtr;
   m = real_to_virtual(fptr);

   count = 0;
   while (m[count] != VBE_MODE_INVAL) {
      count++;
   }

   modes_list_size = (count + 1) * sizeof (vbe_mode_id_t);
   modes_list = sys_malloc(modes_list_size);
   if (modes_list == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   memcpy(modes_list, m, modes_list_size);
   *modes = modes_list;

   return ERR_SUCCESS;
}

/*-- vbe_set_mode --------------------------------------------------------------
 *
 *      Set VBE mode to the specified supported resolution. Only linear/flat
 *      framebuffer modes are supported.
 *
 * Parameters
 *      IN id: VBE mode id
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_set_mode(vbe_mode_id_t id)
{
   vbe_mode_t mode;
   uint16_t flags;
   uintptr_t unused;
   int status;

   status = int10_set_vbe_mode(id | VBE_MODE_ID_ATTR_LINEAR, NULL);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = vbe_get_mode_info(id, &mode, &unused);
   if (status != ERR_SUCCESS) {
      return status;
   }

   flags = COM32_VIDEO_GRAPHIC_MODE | COM32_VIDEO_VESA_MODE |
      COM32_VIDEO_NO_TEXT;

   if (mode.XResolution != 640 || mode.YResolution != 480) {
      flags |= COM32_VIDEO_NON_STANDARD;
   }

   return com32_report_video_mode(flags, mode.XResolution, mode.YResolution);
}

/*-- vbe_get_current_mode ------------------------------------------------------
 *
 *      Return current VBE mode (VBE Specification, Function 03h)
 *
 *      NOTE: VBE 3.0 Specification says:
 *        "This function is not guaranteed to return an accurate mode value if
 *         the mode set was not done with VBE function 02h."
 *
 * Parameters
 *      OUT id: current VBE mode 16-bit id
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_get_current_mode(vbe_mode_id_t *id)
{
   return int10_get_current_vbe_mode(id);
}

/*-- vbe_force_vga_text --------------------------------------------------------
 *
 *      Switch to standard VGA text mode.
 *
 * Parameters
 *      OUT id:   VBE mode identifier
 *      OUT mode: VBE mode information
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vbe_force_vga_text(vbe_mode_id_t *id, vbe_mode_t *mode)
{
   int status;

   status = com32_force_text_mode();
   if (status != ERR_SUCCESS) {
      return status;
   }

   /*
    * vbe_get_current_mode is unlikely to work (and was observed to
    * fail both in a VMware VM and an HP ProLiant ML350p Gen8),
    * because VGA text mode is not a VESA-defined mode and we did not
    * set it using VBE function 02h.  See note in header of
    * vbe_get_current_mode.  So just clear the mode information.
    */
   *id = 3; // VGA 720x400 pixels, 80x25 chars, 16 colors
   memset(mode, 0, sizeof (vbe_mode_t));

   return ERR_SUCCESS;
}
