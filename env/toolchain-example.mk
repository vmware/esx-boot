#*******************************************************************************
# Copyright (c) 2017 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

# Example version of toolchain.mk for esx-boot open source builds.
#                                                                               
# The host toolchain is used to build tools                                     
# like uefi/elf2efi and bios/relocs.                                            
#                                                                               

TCROOT          := /build/toolchain/lin32
GCCVER          := 4.8.0
HOST_CC         := $(TCROOT)/gcc-$(GCCVER)/bin/x86_64-linux5.0-gcc

BINUTILS   := $(TCROOT)/binutils-2.22/x86_64-linux/bin


HOST_LIBBFD     := $(TCROOT)/binutils-2.17.50.0.15/x86_64-linux/lib/libbfd.a
HOST_LIBBFDINC  := $(TCROOT)/binutils-2.17.50.0.15/x86_64-linux/include
HOST_LIBERTY    := $(TCROOT)/binutils-2.17.50.0.15/lib/libiberty.a

GCCROOT := /build/toolchain/lin32/gcc-4.8.0

CC      := $(GCCROOT)/bin/x86_64-linux5.0-gcc
LD      := $(BINUTILS)/ld
AR      := $(BINUTILS)/ar
OBJCOPY := $(BINUTILS)/objcopy
AS      := /build/toolchain/lin32/nasm-2.01/bin/nasm
