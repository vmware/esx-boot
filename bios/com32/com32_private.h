/*******************************************************************************
 * Copyright (c) 2008-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * com32_private.h -- Com32 compatibility library private declarations
 */

#ifndef COM32_PRIVATE_H_
#define COM32_PRIVATE_H_

#include <string.h>
#include <syslog.h>
#include <bootlib.h>
#include <boot_services.h>
#include <bios.h>
#include "com32.h"

#define CONSTRUCTOR   __attribute__((constructor))

typedef struct {
   uint8_t major;
   uint8_t minor;
   uint8_t derivative;
   uint8_t drive;
   bool in_boot_services;
} com32_t;

#define STACK_SIZE       (8 * 1024 * 1024)

EXTERN com32_t com32;
EXTERN struct com32_sys_args __com32;

/*
 *      NOTE: PXELINUX before 3.86 do not provide the module name in the COM32
 *            arguments structure. In this case we return a fake module name.
 */
static INLINE const char *com32_get_modname(void)
{
   static const char *dummy_argv0 = FAKE_ARGV0;

   if (__com32.cs_sysargs > 7 && __com32.cs_name != NULL) {
      return __com32.cs_name;
   } else {
      return dummy_argv0;
   }
}

static INLINE void *get_bounce_buffer(void)
{
   return __com32.cs_bounce;
}

static INLINE size_t get_bounce_buffer_size(void)
{
   return (size_t)__com32.cs_bounce_size;
}

static INLINE bool isSyslinux(void) {
   return (com32.derivative == COM32_DERIVATIVE_SYSLINUX);
}

static INLINE bool isExtlinux(void) {
   return (com32.derivative == COM32_DERIVATIVE_EXTLINUX);
}

static INLINE bool isIsolinux(void) {
   return (com32.derivative == COM32_DERIVATIVE_ISOLINUX);
}

static INLINE bool isPxelinux(void)
{
   return (com32.derivative == COM32_DERIVATIVE_PXELINUX);
}

void intcall(uint8_t vector, const com32sys_t *iregs, com32sys_t *oregs);
int intcall_check_CF(uint8_t vector, const com32sys_t *iregs,
                     com32sys_t *oregs);

/*
 * memory.c
 */
int int12_get_memory_size(size_t *lowmem);
int int15_e801(size_t *s1, size_t *s2);
int int15_88(size_t *size);
int int15_e820(e820_range_t *desc, uint32_t *next, uint32_t *desc_size);

/*
 * malloc.c
 */
void *realloc(void *ptr, size_t size);
void log_malloc_arena(void);

/*
 * disk.c
 */
int get_disk_info(uint8_t drive, disk_t *disk);

/*
 * exec.c
 */
int com32_run_command(const char *command);
int com32_run_default(void);

/*
 * net.c
 */
bool isGPXE(void);

#endif /* !COM32_PRIVATE_H_ */
