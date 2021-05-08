#*******************************************************************************
# Copyright (c) 2008-2011,2014,2020 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Libc Makefile
#

TOPDIR       := ..
include common.mk

SRC         := \
               atexit.c          \
               ctypes.c          \
               generic.c         \
               getopt.c          \
               inet_pton.c       \
               mem.c             \
               printf.c          \
               standard.c        \
               stack_chk.c       \
               stack_chk2.c      \
               string.c

BASENAME    := c
TARGETTYPE  := lib

include rules.mk

$(ODIR)/stack_chk.o: stack_chk.c
	$(call printcmd,CC)
	$(CC) $(CFLAGS) -fno-stack-protector -c -o $@ $<
