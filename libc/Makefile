#*******************************************************************************
# Copyright (c) 2008-2021 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

$(ODIR)/stack_chk2.o: stack_chk2.c
	$(call printcmd,CC)
	$(CC) $(CFLAGS) -fno-stack-protector -c -o $@ $<
