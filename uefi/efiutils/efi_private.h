/*******************************************************************************
 * Copyright (c) 2008-2013,2015-2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * efi_private.h -- EFI boot services private header
 */

#ifndef EFI_PRIVATE_H_
#define EFI_PRIVATE_H_

#include <sys/types.h>
#include <efiutils.h>
#include <boot_services.h>
#include <bootlib.h>
#include <fb.h>

#ifndef EFI_CONSOLE_CONTROL_PROTOCOL_GUID
#   include "protocol/ConsoleControl.h"
#endif
#ifndef APPLE_BOOT_VIDEO_PROTOCOL_GUID
#   include "protocol/apple.h"
#endif

/*
 * init.c
 */
#define WATCHDOG_DISABLE         0
#define WATCHDOG_DEFAULT_TIMEOUT 300   /* 5 minutes, in seconds */

/*
 * init_arch.c
 */
int sanitize_page_tables(void);

/*
 * memory.c
 */
EXTERN UINTN MapKey;

void mem_init(EFI_MEMORY_TYPE MemType);

#define NextMemoryDescriptor(_CurrentDesc_, _DescSize_)                   \
   ((EFI_MEMORY_DESCRIPTOR *)(((UINT8 *)(_CurrentDesc_)) + (_DescSize_)))

EFI_STATUS efi_get_memory_map(UINTN desc_extra_mem,
                              EFI_MEMORY_DESCRIPTOR **MMap,
                              UINTN *Size, UINTN *SizeOfDesc,
                              UINT32 *MMapVersion);
void efi_log_memory_map(efi_info_t *efi_info);

/*
 * volume.c
 */
EFI_STATUS get_boot_partition(int volid, EFI_HANDLE *volume);

/*
 * vbe.c
 */
typedef struct {
   uint16_t width;   /* Width in pixels */
   uint16_t height;  /* Height in pixels */
   uint8_t depth;    /* Depth in bits per pixel */
} resolution_t;

#define VBE_BPP(_pxl_)                              \
   ((_pxl_)->RedSize + (_pxl_)->GreenSize +         \
    (_pxl_)->BlueSize + (_pxl_)->RsvdSize)

#define IS_VBE_PXL_15_BIT(_pxl_)                    \
   (((_pxl_)->RedSize   == 5) &&                    \
    ((_pxl_)->GreenSize == 5) &&                    \
    ((_pxl_)->BlueSize  == 5) &&                    \
    ((_pxl_)->RsvdSize  == 0))

#define IS_VBE_PXL_16_BIT(_pxl_)                    \
   (((_pxl_)->RedSize   == 5) &&                    \
    ((_pxl_)->GreenSize == 6) &&                    \
    ((_pxl_)->BlueSize  == 5) &&                    \
    ((_pxl_)->RsvdSize  == 0))

#define IS_VBE_PXL_24_BIT(_pxl_)                    \
   (((_pxl_)->RedSize   == 8) &&                    \
    ((_pxl_)->GreenSize == 8) &&                    \
    ((_pxl_)->BlueSize  == 8) &&                    \
    ((_pxl_)->RsvdSize  == 0))

#define IS_VBE_PXL_32_BIT(_pxl_)                    \
   (((_pxl_)->RedSize   == 8) &&                    \
    ((_pxl_)->GreenSize == 8) &&                    \
    ((_pxl_)->BlueSize  == 8) &&                    \
    ((_pxl_)->RsvdSize  == 8))

#define IS_VBE_PIXEL(_pxl_)                                     \
   (IS_VBE_PXL_15_BIT(_pxl_) || IS_VBE_PXL_16_BIT(_pxl_) ||     \
    IS_VBE_PXL_24_BIT(_pxl_) || IS_VBE_PXL_32_BIT(_pxl_))

void set_pixel_format(pixel32_t *pxl, uint32_t red, uint32_t green,
                      uint32_t blue, uint32_t reserved);
void efi_clean_vbe(void);

/*
 * gop.c
 */
EFI_STATUS gop_get_fb_info(resolution_t *res, framebuffer_t *fb);
EFI_STATUS gop_set_video_mode(unsigned int w, unsigned int h, unsigned int bpp);
EFI_STATUS gop_init(resolution_t **res, unsigned int *n);

/*
 * uga.c
 */
EFI_STATUS uga_get_fb_info(resolution_t *res, framebuffer_t *fb);
EFI_STATUS uga_set_video_mode(unsigned int w, unsigned int h, unsigned int bpp);
EFI_STATUS uga_init(resolution_t **res, unsigned int *n);

/*
 * net.c
 */
void disable_network_controllers(void);

/*
 * runtime.c
 */
typedef struct rts_policy {
   const char *name;
   int (*supported)(efi_info_t *efi_info,
                    uint64_t *virtualMapSize);
   void (*fill)(efi_info_t *efi_info,
                EFI_MEMORY_DESCRIPTOR *vmap);
   void (*pre_quirk)(efi_info_t *efi_info,
                     EFI_MEMORY_DESCRIPTOR *vmap,
                     uint64_t virtualMapSize);
   void (*post_quirk)(efi_info_t *efi_info,
                      EFI_MEMORY_DESCRIPTOR *vmap,
                      uint64_t virtualMapSize);
   uint64_t incompat_efi_quirks;
   uint64_t efi_caps;
} rts_policy;

extern rts_policy rts_simple;
extern rts_policy rts_simple_generic_quirk;
extern rts_policy rts_sparse;
extern rts_policy rts_compact;
extern rts_policy rts_contig;
void rts_generic_pre(efi_info_t *efi_info,
                     EFI_MEMORY_DESCRIPTOR *vmap,
                     uint64_t virtualMapSize);
void rts_generic_post(efi_info_t *efi_info,
                     EFI_MEMORY_DESCRIPTOR *vmap,
                     uint64_t virtualMapSize);
uint64_t get_l1e_flags(uint64_t *l4pt, uint64_t lpn);

#endif /* !EFI_PRIVATE_H_ */
