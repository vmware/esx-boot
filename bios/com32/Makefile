#*******************************************************************************
# Copyright (c) 2008-2013,2019 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# COM32 abstraction library Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := console.c    \
               disk.c       \
               entry.S      \
               exec.c       \
               exit.S       \
               file.c       \
               init.c       \
               intcall.c    \
               keyboard.c   \
               malloc.c     \
               memory.c     \
               net.c        \
               systab.c     \
	       timer.c      \
               vbe.c        \
               volume.c

BASENAME    := $(FIRMWARE)$(ARCH)
TARGETTYPE  := lib

include rules.mk
