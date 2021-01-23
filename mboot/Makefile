#*******************************************************************************
# Copyright (c) 2008-2017,2020 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Mboot Makefile
#

TOPDIR      := ..
include common.mk

SRC         := config.c              \
               elf.c                 \
               $(IARCH)/elf_arch.c   \
               gui.c                 \
               load.c                \
               mboot.c               \
               $(IARCH)/multiboot.c  \
               esxbootinfo.c         \
               $(IARCH)/esxbootinfo_arch.c \
               reloc.c               \
               secure.c              \
               system.c              \
               $(IARCH)/system_arch.c\

ifeq ($(IARCH),x86)
#
# NASM source - x86 and x64.
#
SRC += $(IARCH)/trampoline.s
else
#
# GNU AS source - arm64.
#
SRC += $(IARCH)/trampoline.S
endif

TARGETTYPE  := app
INC += .

LIBS := $(BOOTLIB) $(ENV_LIB)

ifeq (,$(EKEY))
   BASENAME := mboot
else
   BASENAME := mboot-$(EKEY)
   CFLAGS   += -D$(EKEY) -DSECURE_BOOT
   INC      += $(UEFIINC) $(CRYPTOINC)

   ifeq ($(CRYPTO_MODULE), 1)
      CFLAGS += -DCRYPTO_MODULE
   else
      LIBS   += $(CRYPTOLIB)
   endif
endif

include rules.mk
