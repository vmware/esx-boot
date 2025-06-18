/*******************************************************************************
 * Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * com32.h -- COM32 API
 */

#ifndef COM32_H_
#define COM32_H_

#include <sys/types.h>
#include <compat.h>

#pragma pack(1)
typedef union {
    uint32_t l;
    uint16_t w[2];
    uint8_t b[4];
} reg32_t;

typedef struct {
    uint16_t gs;
    uint16_t fs;
    uint16_t es;
    uint16_t ds;
    reg32_t edi;
    reg32_t esi;
    reg32_t ebp;
    reg32_t _unused_esp;
    reg32_t ebx;
    reg32_t edx;
    reg32_t ecx;
    reg32_t eax;
    reg32_t eflags;
} com32sys_t;

struct com32_pmapi;

struct com32_sys_args {
    uint32_t cs_sysargs;
    char *cs_cmdline;
    void CDECL (*cs_intcall)(uint8_t, const com32sys_t *, com32sys_t *);
    void *cs_bounce;
    uint32_t cs_bounce_size;
    void CDECL (*cs_farcall)(uint32_t, const com32sys_t *, com32sys_t *);
    int CDECL (*cs_cfarcall)(uint32_t, const void *, uint32_t);
    uint32_t cs_memsize;
    const char *cs_name;
    const struct com32_pmapi *cs_pm;
};
#pragma pack()

#define COM32_INT_DOS_COMPATIBLE  0x21
#define COM32_INT                 0x22

enum com32_derivative {
   COM32_DERIVATIVE_SYSLINUX = 0x31,
   COM32_DERIVATIVE_PXELINUX = 0x32,
   COM32_DERIVATIVE_ISOLINUX = 0x33,
   COM32_DERIVATIVE_EXTLINUX = 0x34,
   COM32_DERIVATIVE_GPXE     = 0x46
};

#endif /* COM32_H_ */
