#*******************************************************************************
# Copyright (c) 2015-2021 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# mbedtls Makefile
#

TOPDIR       := ..
include common.mk

SRC         := bignum.c \
               md.c \
               md_wrap.c \
               oid.c \
               rsa.c \
               sha256.c \
               sha512.c \
               platform_util.c \
               rsa_internal.c

BASENAME    := mbedtls
TARGETTYPE  := lib
CFLAGS      += -Dasm=__asm__

include rules.mk
