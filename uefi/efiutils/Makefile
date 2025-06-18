#*******************************************************************************
# Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Boot Services Library Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := acpi_table.c \
               console.c    \
               devpath.c    \
               devpathl.c   \
               disk.c       \
               efi_main.c   \
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
               inboot.c     \
               init.c       \
               keyboard.c   \
               loadfile.c   \
               logbuf.c     \
               memory.c     \
               net.c        \
               protocol.c   \
               protocoll.c  \
               quirks.c     \
               runtime.c    \
               runtime_generic.c \
               runtime_simple.c \
               runtime_sparse.c \
               runtime_compact.c \
               runtime_contig.c \
               runtime_watchdog.c\
               simplefile.c \
               systab.c     \
               tcg2.c       \
               tftpfile.c   \
               timer.c      \
               tpm.c        \
               tpm2_nv.c    \
               tpm2_marshal.c \
               ucs2.c       \
               ucs2a.c      \
               uga.c        \
               urlresolve.c \
               vbe.c        \
               volume.c     \
               $(IARCH)/console_arch.c \
               $(IARCH)/init_arch.c \

BASENAME    := $(FIRMWARE)$(ARCH)
TARGETTYPE  := lib
INC         := $(UEFIINC) ./ $(UEFIPROTO)

include rules.mk

$(ODIR)/efi_main.o: efi_main.c
	$(call printcmd,CC)
	$(CC) $(CFLAGS) -fno-stack-protector -c -o $@ $<
