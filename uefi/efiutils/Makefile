#*******************************************************************************
# Copyright (c) 2008-2013,2015-2016,2019-2020 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Boot Services Library Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := console.c    \
               devpath.c    \
               dhcpv4.c     \
               disk.c       \
               efi_secure.c \
               error.c      \
               exitboot.c   \
               file.c       \
               gop.c        \
               gpxefile.c   \
               guid.c       \
               httpfile.c   \
               ishttp.c     \
               image.c      \
               init.c       \
               keyboard.c   \
               loadfile.c   \
               memory.c     \
               net.c        \
               protocol.c   \
               quirks.c     \
               runtime.c    \
               runtime_generic.c \
               runtime_simple.c \
               runtime_sparse.c \
               runtime_compact.c \
               runtime_contig.c \
               simplefile.c \
               systab.c     \
               tcg2.c       \
               tftpfile.c   \
               timer.c      \
               tpm.c        \
               ucs2.c       \
               uga.c        \
               vbe.c        \
               volume.c     \
               $(IARCH)/console_arch.c \
               $(IARCH)/init_arch.c \

BASENAME    := $(FIRMWARE)$(ARCH)
TARGETTYPE  := lib
INC         := $(UEFIINC) ./

include rules.mk
