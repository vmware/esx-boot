#*******************************************************************************
# Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# ISO9660 driver Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := fsw_efi.c     \
               fsw_efi_lib.c \
               fsw_core.c    \
               fsw_lib.c     \
               fsw_iso9660.c

BASENAME    := iso9660
TARGETTYPE  := bsdrv
CDEF        := FSTYPE=iso9660 FSW_DEBUG_LEVEL=0 HOST_EFI
INC         := $(UEFIINC)
LIBS        := $(ENV_LIB) $(BOOTLIB)

include rules.mk
