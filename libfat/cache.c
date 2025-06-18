/*******************************************************************************
 * Copyright (c) 2011-2016 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * cache.c
 *
 * Simple sector cache
 */

#include <stdlib.h>
#include "libfatint.h"

void *libfat_get_sector(struct libfat_filesystem *fs, libfat_sector_t n)
{
    struct libfat_sector *ls;

    for (ls = fs->sectors; ls; ls = ls->next) {
	if (ls->n == n)
	    return ls->data;	/* Found in cache */
    }

    /* Not found in cache */
    ls = malloc(sizeof(struct libfat_sector));
    if (!ls) {
	libfat_flush(fs);
	ls = malloc(sizeof(struct libfat_sector));

	if (!ls)
	    return NULL;	/* Can't allocate memory */
    }

    ls->data = malloc(fs->bytes_per_sector);
    if (ls->data == NULL) {
	free(ls);
	return NULL;
    }

    if (fs->read(fs->readptr, ls->data, fs->bytes_per_sector, n)
	!= (int) fs->bytes_per_sector) {
	free(ls->data);
	free(ls);
	return NULL;		/* I/O error */
    }

    ls->n = n;
    ls->next = fs->sectors;
    fs->sectors = ls;

    return ls->data;
}

void libfat_flush(struct libfat_filesystem *fs)
{
    struct libfat_sector *ls, *lsnext;

    lsnext = fs->sectors;
    fs->sectors = NULL;

    for (ls = lsnext; ls; ls = lsnext) {
	lsnext = ls->next;
	free(ls);
    }
}
