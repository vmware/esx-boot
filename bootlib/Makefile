#*******************************************************************************
# Copyright (c) 2008-2013,2016,2018 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Boot utils Library Makefile
#

TOPDIR       := ..
include common.mk

SRC         := alloc.c       \
               e820.c        \
               error.c       \
               fb.c          \
               fbcon.c       \
               file.c        \
               gpt.c         \
               gzip.c        \
               log.c         \
               mbr.c         \
	       net.c         \
               parse.c       \
               serial.c      \
               sort.c        \
               string.c      \
               video.c       \
               volume.c      \
               smbios.c

BASENAME    := boot
TARGETTYPE  := lib
INC         := $(ZLIB_INC) $(LIBFAT_INC)

include rules.mk
