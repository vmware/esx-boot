#*******************************************************************************
# Copyright (c) 2008-2011,2022 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Libz Makefile
#

TOPDIR      := ..
include common.mk

SRC         := adler32.c        \
               crc32.c          \
               inffast.c        \
               inflate.c        \
               inftrees.c       \
               uncompr.c        \
               zutil.c

BASENAME    := z
TARGETTYPE  := lib
CDEF        =  NO_GZCOMPRESS NO_STRERROR
CFLAGS      += -Wno-strict-prototypes

include rules.mk
