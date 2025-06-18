#*******************************************************************************
# Copyright (c) 2020 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# FDT Library Makefile
#

TOPDIR       := ..
include common.mk

SRC         := fdt.c     \
               fdt_ro.c  \
               fdt_wip.c \
               fdt_sw.c  \
               fdt_rw.c  \
               fdt_strerror.c \
               fdt_empty_tree.c \
               fdt_addresses.c \
               fdt_overlay.c \
               fdt_check.c \
               fdt_vmware.c

BASENAME    := fdt
TARGETTYPE  := lib
INC         := $(FDTINC)

include rules.mk
