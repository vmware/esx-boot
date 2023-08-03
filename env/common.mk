#*******************************************************************************
# Copyright (c) 2008-2015,2019-2022 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
#-- Makefile utils -------------------------------------------------------------
#

ifndef VERBOSE
   # VERBOSE not set: Terse messages; can be hard to debug makefile issues.
   MAKEFLAGS += -s
else
ifeq (0,$(VERBOSE))
   # VERBOSE=0: Same as VERBOSE not set.
   MAKEFLAGS += -s
else
ifeq (1,$(VERBOSE))
   # VERBOSE=1: Medium verbose messages.
   MAKEFLAGS += --no-print-directory
else
   # VERBOSE=2 (or other value): Maximally verbose messages.
endif
endif
endif

ECHO       := @echo
print      =  $(ECHO) "[$(BUILDENV) $(1)] $(patsubst $(BUILD_DIR)/%,%,$(2))"

RMDIR      =  $(ECHO) "[RMDIR] $(1)" ; rm -rf $(1)
MKDIR      =  @mkdir -p $(1)
UNTGZ      =  $(ECHO) "[UNTARGZ] $(1)" ; tar xzf $(1) -C $(2)

ifdef BUILDENV

#
#-- Environment ----------------------------------------------------------------
#
ifeq ($(BUILDENV),com32)
   FIRMWARE   := bios
   APP_EXT    := .c32
   ARCH       := ia32
else
   FIRMWARE   := uefi
   APP_EXT    := .efi-$(KEY)

   ifeq ($(BUILDENV),uefiarm64)
      ARCH       := arm64
   else ifeq ($(BUILDENV),uefiriscv64)
      ARCH       := riscv64
   else ifeq ($(BUILDENV),uefi64)
      ARCH       := em64t
   else ifeq ($(BUILDENV),uefi32)
      ARCH       := ia32
   else
      $(error ERROR: $(BUILDENV): Unsupported build environment)
   endif
endif

#
#-- Tool chain -----------------------------------------------------------------
#
include $(TOPDIR)/env/toolchain.mk

#
#-- Tools ----------------------------------------------------------------------
#
TOOLS_DIR  := $(TOPDIR)/build/tools

ifeq ($(FIRMWARE),bios)
   RELOCS     := $(TOOLS_DIR)/relocs
else
   ifeq ($(ARCH),riscv64)
      ELF2EFI    := $(TOOLS_DIR)/elf2efi-riscv64
   else ifeq ($(ARCH),arm64)
      ELF2EFI    := $(TOOLS_DIR)/elf2efi-arm64
   else ifeq ($(ARCH),em64t)
      ELF2EFI    := $(TOOLS_DIR)/elf2efi64
   else ifeq ($(ARCH),ia32)
      ELF2EFI    := $(TOOLS_DIR)/elf2efi32
   endif
endif

#
#-- Compilation flags ----------------------------------------------------------
#
ifeq ($(ARCH),riscv64)
   IARCH      := riscv64
   UEFIARCH   := RiscV64
   AFLAGS     :=
   CFLAGS     := -march=rv64gc -mabi=lp64d -fno-short-enums -fsigned-char \
                 -ffunction-sections -fdata-sections -fomit-frame-pointer \
                 -fno-builtin -Wno-address -mcmodel=medany -mno-relax \
                 -fpack-struct=8 -fno-asynchronous-unwind-tables \
                 -fno-builtin-memcpy -fno-stack-protector
   LDFLAGS    :=
else ifeq ($(ARCH),arm64)
   IARCH      := arm64
   UEFIARCH   := AArch64
   AFLAGS     :=
   CFLAGS     := -march=armv8-a -mlittle-endian -mgeneral-regs-only \
                 -mcmodel=large -fno-short-enums -fsigned-char \
                 -ffunction-sections -fdata-sections -fomit-frame-pointer \
                 -fno-builtin -Wno-address -mstack-protector-guard=global
   LDFLAGS    :=
else ifeq ($(ARCH),em64t)
   IARCH      := x86
   UEFIARCH   := X64
   AFLAGS     := -f elf64
   CFLAGS     := -m64 -mno-red-zone -msoft-float -DNO_MSABI_VA_FUNCS \
                 -mstack-protector-guard=global
   LDFLAGS    := -m elf_x86_64
else ifeq ($(ARCH),ia32)
   IARCH      := x86
   UEFIARCH   := Ia32
   AFLAGS     := -f elf32
   CFLAGS     := -m32 -march=i386 -mpreferred-stack-boundary=2 -msoft-float \
                 -mstack-protector-guard=global
   LDFLAGS    := -m elf_i386
endif

ifeq ($(FIRMWARE),bios)
   CFLAGS     += -fPIE -fno-asynchronous-unwind-tables -D__COM32__ -msoft-float
   LDSCRIPT   := $(TOPDIR)/bios/com32/com32.ld
else
   CFLAGS     += -fshort-wchar -fno-dwarf2-cfi-asm
   LDFLAGS    += --defsym _start=efi_main
   LDSCRIPT   := $(TOPDIR)/uefi/uefi.lds
   ifeq ($(ARCH),ia32)
      CFLAGS     += -malign-double -fno-pie
   else ifeq ($(ARCH),em64t)
      CFLAGS     += -fpie
   endif
endif

ifeq ($(DEBUG),1)
CFLAGS    += -DDEBUG
endif

CFLAGS     += --sysroot=$(GCCROOT) -Donly_$(ARCH) -Donly_$(IARCH)            \
              -DVMWARE_EDK2_CHANGES                                          \
              -ffreestanding -fno-exceptions                                 \
                                                                             \
              -Os -fstrength-reduce -ffast-math -fomit-frame-pointer         \
              -finline-limit=2000 -freg-struct-return                        \
              -fno-strict-aliasing -falign-functions=0 -falign-jumps=0       \
              -falign-labels=0 -falign-loops=0 -fwrapv -fvisibility=hidden   \
              -fstack-protector -fstack-protector-all                        \
                                                                             \
              -W -Wall -Werror -std=c99 -Wwrite-strings -Wstrict-prototypes  \
              -Wpointer-arith -Wdeclaration-after-statement                  \
              -Wvla -Woverlength-strings -Wredundant-decls -Wformat          \
              -Wno-implicit-fallthrough

LDFLAGS    += -nostdlib -q -T $(LDSCRIPT)

# Use this to omit debug symbols from the ELF file:
#LDFLAGS    += -S
#CFLAGS     +=

# Use this to include debug symbols in the ELF file:
LDFLAGS    +=
CFLAGS     += -g

# Use this to get a map during linking:
#LDFLAGS += -M

#
#-- Files locations ------------------------------------------------------------
#
BUILD_DIR  := $(TOPDIR)/build/$(BUILDENV)

LIB_DIR    := $(BUILD_DIR)/lib
LIBBP      := $(LIB_DIR)/bp/libbp.a
LIBC       := $(LIB_DIR)/c/libc.a
LIBFAT     := $(LIB_DIR)/fat/libfat.a
LIBCRC     := $(LIB_DIR)/crc/libcrc.a
LIBMD5     := $(LIB_DIR)/md5/libmd5.a
LIBUART    := $(LIB_DIR)/uart/libuart.a
ZLIB       := $(LIB_DIR)/z/libz.a
LIBGCC     := $(shell $(CC) $(CFLAGS) --print-libgcc)
BOOTLIB    := $(LIB_DIR)/boot/libboot.a
FIRMLIB    := $(LIB_DIR)/$(FIRMWARE)$(ARCH)/lib$(FIRMWARE)$(ARCH).a
CRYPTOLIB  := $(LIB_DIR)/mbedtls/libmbedtls.a
FDTLIB     := $(LIB_DIR)/fdt/libfdt.a

ENV_LIB    := $(FIRMLIB) $(LIBFAT) $(LIBC) $(LIBCRC) $(LIBMD5) $(LIBUART) \
              $(ZLIB) $(LIBGCC) $(LIBBP)

LIBMD5_INC := $(TOPDIR)/libmd5
STDINC     := $(TOPDIR)/libc/include $(TOPDIR)/include $(TOPDIR)/include/$(IARCH) $(LIBMD5_INC)
ZLIB_INC   := $(TOPDIR)/zlib
LIBFAT_INC := $(TOPDIR)/libfat
EDK2INC    := $(TOPDIR)/uefi/edk2
UEFIINC    := $(TOPDIR)/uefi                        \
              $(EDK2INC)/MdePkg/Include             \
              $(EDK2INC)/MdePkg/Include/$(UEFIARCH) \
              $(EDK2INC)/MdePkg/Include/Protocol    \
              $(EDK2INC)/EmbeddedPkg/Include
CRYPTOINC  := $(TOPDIR)/mbedtls/mbedtls $(TOPDIR)/uefi/efiutils
FDTINC     := $(TOPDIR)/libfdt
BPINC      := $(TOPDIR)/libbp

ifneq ($(filter arm64 riscv64,$(ARCH)),)
ENV_LIB += $(FDTLIB)
STDINC += $(FDTINC)
endif

endif # !BUILDENV
