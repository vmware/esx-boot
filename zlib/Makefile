#*******************************************************************************
# Copyright (c) 2008-2022 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
