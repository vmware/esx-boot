#*******************************************************************************
# Copyright (c) 2008-2013,2015-2016 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Boot Services Library Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := console.c    \
               devpath.c    \
               disk.c       \
               efi_secure.c \
               error.c      \
               file.c       \
               gop.c        \
               gpxefile.c   \
               image.c      \
               init.c       \
               keyboard.c   \
               loadfile.c   \
               memory.c     \
               net.c        \
               protocol.c   \
               runtime.c    \
               runtime_generic.c \
               runtime_simple.c \
               runtime_sparse.c \
               runtime_compact.c \
               runtime_contig.c \
               simplefile.c \
               systab.c     \
               tftpfile.c   \
               timer.c      \
               ucs2.c       \
               uga.c        \
               vbe.c        \
               volume.c     \
               quirks.c     \
               $(IARCH)/console_arch.c \
               $(IARCH)/init_arch.c \

BASENAME    := $(FIRMWARE)$(ARCH)
TARGETTYPE  := lib
INC         := $(UEFIINC) ./

include rules.mk
