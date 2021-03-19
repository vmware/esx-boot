#*******************************************************************************
# Copyright (c) 2008-2012,2015,2017-2019 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Tool chain definitions.
# Example for building with the VMware ODP toolchain.
#

TCROOT          := /build/toolchain/lin32

#=============================================================
# Host definitions. Used to compile tools that run on the
# host during the build, such as uefi/elf2efi and bios/relocs.
#=============================================================

HOST_CC         := $(TCROOT)/gcc-4.8.0/bin/x86_64-linux5.0-gcc
HOST_CFLAGS     := -m32
#
# Known to work with libbfd 2.17.50. libbfd
# used should be aware of all architecture types
# used in build.
#
HOST_LIBBFDINC  := $(TCROOT)/binutils-2.17.50.0.15/x86_64-linux/include
HOST_LIBBFD     := $(TCROOT)/binutils-2.17.50.0.15/x86_64-linux/lib/libbfd.a
HOST_LIBERTY    := $(TCROOT)/binutils-2.17.50.0.15/lib/libiberty.a
HOST_CRYPTO     := -lcrypto

#=============================================================
# Target definitions.
#=============================================================

ifeq ($(BUILDENV),uefiarm64)
GCCROOT := <Path to aarch64 gcc sysroot>
CC      := <Path to aarch64 gcc>
LD      := <Path to aarch64 ld>
AR      := <Path to aarch64 ar>
OBJCOPY := <Path to aarch64 objcopy>
else
#
# Known to work with gcc 4.8.0, nasm 2.01, and binutils 2.22
#
GCCROOT := $(TCROOT)/gcc-4.8.0
CC      := $(GCCROOT)/bin/x86_64-linux5.0-gcc
BINUTILS:= $(TCROOT)/binutils-2.22/x86_64-linux/bin
LD      := $(BINUTILS)/ld
AR      := $(BINUTILS)/ar
OBJCOPY := $(BINUTILS)/objcopy
AS      := $(TCROOT)/nasm-2.01/bin/nasm
endif
