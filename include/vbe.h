/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * vbe.h -- VBE 3.0 specification definitions
 */

#ifndef VBE_H_
#define VBE_H_

#include <sys/types.h>

typedef uint32_t vbeFarPtr_t;

#define VBE_VERSION                    0x300

#define VBE_CAP_DAC_SWITCHABLE_WIDTH   (1 << 0)
#define VBE_CAP_NO_VGA                 (1 << 1)
#define VBE_CAP_OLD_RAMDAC             (1 << 2)
#define VBE_CAP_STEREOSCOPIC           (1 << 3)
#define VBE_CAP_EVC_STEREO_CONNECTOR   (1 << 4)

#define VESA_MAGIC ('V' + ('E' << 8) + ('S' << 16) + ('A' << 24))
#define VBE2_MAGIC ('V' + ('B' << 8) + ('E' << 16) + ('2' << 24))

typedef uint16_t  vbe_mode_id_t;

/* VESA General Information table */
#pragma pack(1)
typedef struct {
   uint32_t VbeSignature;
   uint16_t VbeVersion;
   vbeFarPtr_t OemStringPtr;
   uint32_t Capabilities;
   vbeFarPtr_t VideoModePtr;
   uint16_t TotalMemory;
   uint16_t OemSoftwareRev;
   vbeFarPtr_t OemVendorNamePtr;
   vbeFarPtr_t OemProductNamePtr;
   vbeFarPtr_t OemProductRevPtr;
   uint8_t Reserved[222];
   uint8_t OemData[256];
} vbe_t;
#pragma pack()

#define VBE_MODE_ATTR_AVAILABLE        (1 << 0)
#define VBE_MODE_ATTR_VBE12_EXTENSION  (1 << 1)
#define VBE_MODE_ATTR_BIOS_TTY         (1 << 2)
#define VBE_MODE_ATTR_COLOR            (1 << 3)
#define VBE_MODE_ATTR_GRAPHIC          (1 << 4)
#define VBE_MODE_ATTR_NON_VGA          (1 << 5)
#define VBE_MODE_ATTR_NO_WINDOW        (1 << 6)
#define VBE_MODE_ATTR_LINEAR           (1 << 7)
#define VBE_MODE_ATTR_DOUBLE_SCAN      (1 << 8)
#define VBE_MODE_ATTR_INTERLACED       (1 << 9)
#define VBE_MODE_ATTR_TRIPLE_BUFFER    (1 << 10)
#define VBE_MODE_ATTR_STEREOSCOPIC     (1 << 11)
#define VBE_MODE_ATTR_DUAL             (1 << 12)

#define VBE_MEMORY_MODEL_TEXT          0
#define VBE_MEMORY_MODEL_CGA           1
#define VBE_MEMORY_MODEL_HERCULE       2
#define VBE_MEMORY_MODEL_PLANAR        3
#define VBE_MEMORY_MODEL_PACKED_PIXEL  4
#define VBE_MEMORY_MODEL_NON_CHAIN     5
#define VBE_MEMORY_MODEL_DIRECT_COLOR  6
#define VBE_MEMORY_MODEL_YUV           7

#define VBE_WINDOW_RELOCATABLE         (1 << 0)
#define VBE_WINDOW_READABLE            (1 << 1)
#define VBE_WINDOW_WRITABLE            (1 << 2)

#define VBE_DIRECT_COLOR_PROGRAMMABLE  (1 << 0)
#define VBE_DIRECT_COLOR_RSVD_USABLE   (1 << 1)

#define VBE_MODE_ID_ATTR_VESA          (1 << 8)
#define VBE_MODE_ID_ATTR_USER_CRTC     (1 << 11)
#define VBE_MODE_ID_ATTR_LINEAR        (1 << 14)
#define VBE_MODE_ID_ATTR_DONTCLEAR     (1 << 15)

#define VBE_MODE_INVAL                 0xffff

#pragma pack(1)
typedef struct {
   uint16_t ModeAttributes;
   uint8_t WinAAttributes;
   uint8_t WinABttributes;
   uint16_t WinGranularity;
   uint16_t WinSize;
   uint16_t WinASegment;
   uint16_t WinBSegment;
   vbeFarPtr_t WinFuncPtr;
   uint16_t BytesPerScanLine;

   uint16_t XResolution;
   uint16_t YResolution;
   uint8_t XCharSize;
   uint8_t YCharSize;
   uint8_t NumberOfPlanes;
   uint8_t BitsPerPixel;
   uint8_t NumberOfBanks;
   uint8_t MemoryModel;
   uint8_t BankSize;
   uint8_t NumberOfImagePages;
   uint8_t Reserved0;

   uint8_t RedMaskSize;
   uint8_t RedFieldPosition;
   uint8_t GreenMaskSize;
   uint8_t GreenFieldPosition;
   uint8_t BlueMaskSize;
   uint8_t BlueFieldPosition;
   uint8_t RsvdMaskSize;
   uint8_t RsvdFieldPosition;
   uint8_t DirectColorModeInfo;

   uint32_t PhysBasePtr;
   uint32_t Reserved1;
   uint16_t Reserved2;

   uint16_t LinBytesPerScanLine;
   uint8_t BnkNumberOfImagePages;
   uint8_t LinNumberOfImagePages;
   uint8_t LinRedMaskSize;
   uint8_t LinRedFieldPosition;
   uint8_t LinGreenMaskSize;
   uint8_t LinGreenFieldPosition;
   uint8_t LinBlueMaskSize;
   uint8_t LinBlueFieldPosition;
   uint8_t LinRsvdMaskSize;
   uint8_t LinRsvdFieldPosition;
   uint32_t MaxPixelClock;

   uint8_t Reserved3[189];
} vbe_mode_t;

typedef struct {
   uint16_t HorizontalTotal;
   uint16_t HorizontalSyncStart;
   uint16_t HorizontalSyncEnd;
   uint16_t VerticalTotal;
   uint16_t VerticalSyncStart;
   uint16_t VerticalSyncEnd;
   uint8_t Flags;
   uint32_t PixelClock;
   uint16_t RefreshRate;
   uint8_t Reserved[40];
} vbe_crtc_t;

#pragma pack()

#endif /* !VBE_H_ */
