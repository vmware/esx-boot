/*******************************************************************************
 * Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * apple.h -- Apple-specific EFI protocols definitions
 *
 *      This protocol is available on Apple platforms. It is an UGA extension
 *      which allows to retrieve the framebuffer properties.
 */

#ifndef EFI_APPLE_H_
#define EFI_APPLE_H_

#define APPLE_BOOT_VIDEO_PROTOCOL_GUID \
   { 0xe316e100, 0x0751, 0x4c49, {0x90, 0x56, 0x48, 0x6C, 0x7e, 0x47, 0x29, 0x03} }

typedef struct _APPLE_BOOT_VIDEO_PROTOCOL _APPLE_BOOT_VIDEO_PROTOCOL;

typedef EFI_STATUS (EFIAPI *APPLE_BOOT_VIDEO_GET_FRAMEBUFFER) (
   IN struct _APPLE_BOOT_VIDEO_PROTOCOL *This,
   OUT UINT32 *BaseAddr,
   OUT UINT32 *Size,
   OUT UINT32 *BytesPerRow,
   OUT UINT32 *Width,
   OUT UINT32 *Height,
   OUT UINT32 *ColorDepth);

typedef struct _APPLE_BOOT_VIDEO_PROTOCOL {
   APPLE_BOOT_VIDEO_GET_FRAMEBUFFER GetFramebuffer;
} APPLE_BOOT_VIDEO_PROTOCOL;

#endif /* !EFI_APPLE_H_ */
