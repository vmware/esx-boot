/*******************************************************************************
 * Copyright (c) 2008-2016 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * safeboot.h -- safeboot module header file
 */

#ifndef SAFEBOOT_H_
#define SAFEBOOT_H_

#include <stdbool.h>
#include <sys/types.h>
#include <bootlib.h>

#define ASSERT(_cond_)

#define SAFEBOOT_CFG                "/boot.cfg"

#define VMWARE_FAT_UUID_LEN         16

typedef struct {
   char *self_path;
   char *next_loader;
   char *extra_args;
   framebuffer_t fb;
   bool rollback;
   bool verbose;
   bool serial;
   bool fake_write_err;
   int serial_com;
   int serial_speed;
} safeboot_env_t;

EXTERN safeboot_env_t safeboot;

/*
 * bootbank.c
 */
typedef enum {
   BANK_STATE_UNDEFINED = -1,
   BANK_STATE_VALID     =  0,
   BANK_STATE_UPGRADING =  1,
   BANK_STATE_DIRTY     =  2,
   BANK_STATE_INVALID   =  3
} bank_state_t;

typedef struct {
   int bootstate;
   uint32_t updated;
   char *build;
   int volid;
   unsigned char uuid[VMWARE_FAT_UUID_LEN];
   bool upgrading;
   bool valid;
   bool quickboot;
} bootbank_t;

const char *bootstate_to_str(int bootstate);
int get_boot_bank(bool force_rollback, bootbank_t **bootbank);
void bank_clean(void);

/*
 * chainload.c
 */
int chainload(bootbank_t *bank, int *retval);

/*
 * config.c
 */
int bank_get_config(bootbank_t *bank);
int bank_set_bootstate(bootbank_t *bank, int bootstate);

/*
 * uuid.c
 */
int vmfat_uuid_to_str(const unsigned char *uuid, char **uuid_str);
int vmfat_get_uuid(int volid, void *buffer, size_t buflen);

/*
 * gui.c
 */
bool gui_rollback(void);
int gui_resume_default_boot(void);
int gui_init(void);

#endif /* !SAFEBOOT_H_ */
