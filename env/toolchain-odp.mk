#*******************************************************************************
# Copyright (c) 2008-2020 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

HOST_CC         := gcc
HOST_CFLAGS     := 
HOST_LIBCRYPTO  := -lcrypto

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

GETKEYS_PYTHONPATH := $(PYTHONPATH):$(TCROOT)/lin64/psutil-0.6.1/lib/python2.7/site-packages
