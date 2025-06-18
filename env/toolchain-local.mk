#*******************************************************************************
# Copyright (c) 2008-2022 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Tool chain definitions.
# Example for building with the default tools on the local machine.
#

#=============================================================
# Host definitions. Used to compile tools that run on the
# host during the build, such as uefi/elf2efi and bios/relocs.
#=============================================================

HOST_CC         := gcc
ifeq ($(BUILDENV),uefi32)
HOST_CFLAGS     := -m32 -DEFI_TARGET32
else
HOST_CFLAGS     := -m64 -DEFI_TARGET64
endif
HOST_LIBCRYPTO  := -lcrypto

#=============================================================
# Target definitions.
#=============================================================

# Either python2 or python3 can be used
PYTHON  := /usr/bin/python3
GETKEYS_PYTHONPATH := $(PYTHONPATH)

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
GCCROOT := '/'
CC      := gcc
LD      := ld
AR      := ar
OBJCOPY := objcopy
AS      := nasm
endif
