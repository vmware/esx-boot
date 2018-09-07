/*******************************************************************************
 * Copyright (c) 2015-2017 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * frobosboot.h -- frobos native chain boot header file
 */

#ifndef FROBOSBOOT_H_
#define FROBOSBOOT_H_

#include <sys/types.h>
#include <bootlib.h>
#include <efiutils.h>
#include <boot_services.h>
#include <stdlib.h>

#define BOOT_CFG_LEN 100

extern char **test_list;
extern uint32_t bootoption;
extern uint32_t num_test;

/*
 * ui.c
 */
int setup_display(void);
uint32_t wait_for_bootoption(void);

/*
 * config.c
 */
int frobos_get_next_boot(char *);

#endif /* !FROBOSBOOT_H_ */
