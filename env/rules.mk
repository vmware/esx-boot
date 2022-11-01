#*******************************************************************************
# Copyright (c) 2008-2012,2015,2020-2021 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# rules.mk
#

# Source filenames to object filenames convertion
src_to_obj  =  $(addsuffix .o,$(basename $(1)))
objects     =  $(addprefix $(2),$(call src_to_obj,$(1)))

get_odir    =  $(if $(findstring lib,$(2)),$(LIB_DIR)/$(1),$(BUILD_DIR)/$(1))
get_target  =  $(if $(findstring lib,$(2)),lib$(1).a,$(1)_$(ARCH)$(APP_EXT))

printcmd    =  $(call print,$(1),$@)

# Build variables
ODIR        := $(call get_odir,$(BASENAME),$(TARGETTYPE))
TARGET      := $(ODIR)/$(call get_target,$(BASENAME),$(TARGETTYPE))
OBJECTS     := $(call objects,$(SRC),$(ODIR)/)
CFLAGS      += $(patsubst %,-D%,$(CDEF)) $(patsubst %,-I%,$(STDINC) $(INC))

# Makefile rules
.SECONDARY:
.SUFFIXES: .c .s .S .o .a .elf .efi .json $(APP_EXT)

$(ODIR)/%.c: %.json $(TOPDIR)/env/getkeys.py
	$(call printcmd,GETKEYS)
	LD_LIBRARY_PATH=$(HOST_OPENSSL_LIB):$(LD_LIBRARY_PATH) \
	PYTHONPATH=$(GETKEYS_PYTHONPATH) \
		$(PYTHON) $(TOPDIR)/env/getkeys.py < $< > $@

$(ODIR)/%.o: %.c
	$(call printcmd,CC)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/%.o: %.S
	$(call printcmd,GAS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/%.o: %.s
	$(call printcmd,AS)
	$(AS) $(AFLAGS) -o $@ $<

$(ODIR)/%.a: $(OBJECTS)
	$(call printcmd,AR)
	$(AR) crs $@ $^

$(ODIR)/%.elf: $(OBJECTS) $(LIBS)
	$(call printcmd,LD)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) -\( $(LIBS) -\)

ifeq ($(FIRMWARE),uefi)
# UEFI rules

   ifeq ($(TARGETTYPE),app)
   EFISUBSYSTEM := 10
   else
   ifeq ($(TARGETTYPE),bsdrv)
   EFISUBSYSTEM := 11
   endif
   endif

%.efi: %.elf
	$(call printcmd,ELF2EFI)
	frozen="$(realpath $(TOPDIR)/frozen/$(notdir $@))" ; \
	if [ "$(OBJDIR)" = release -a -f "$$frozen" ] ; then \
	   echo "Substituting frozen binary $$frozen" ; \
	   cp --no-preserve=mode $$frozen $@ ; \
	else \
	   $(ELF2EFI) --subsystem=$(EFISUBSYSTEM) $(ELF2EFIFLAGS) $< $@ ; \
	fi

%.efi-$(KEY): %.efi
	$(call printcmd,SIGN)
	$(PYTHON) $(TOPDIR)/env/sign.py $(KEY) $< $@

else
# Non-UEFI rules

%$(APP_EXT): %.elf
	$(call printcmd,OBJCOPY)
	$(OBJCOPY) -O binary $< $@
	$(call printcmd,RELOCS)
	$(RELOCS) $< >> $@
endif

all: dirs $(TARGET)

dirs:
	$(call MKDIR,$(BUILD_DIR))
	$(call MKDIR,$(ODIR))
	$(call MKDIR,$(ODIR)/$(IARCH))
