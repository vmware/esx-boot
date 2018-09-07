/*******************************************************************************
 * Portions Copyright (c) 2010 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*-
 * Copyright (c) 2006 Christoph Pfisterer
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <string.h>

static int fsw_strcaseeq_ISO88591_UTF16(void *s1data, void *s2data, int len)
{
    int i;
    fsw_u8 *p1 = (fsw_u8 *)s1data;
    fsw_u16 *p2 = (fsw_u16 *)s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = toupper(*p1++);
        c2 = ucs2_toupper(*p2++);
        if (c1 != c2)
            return 0;
    }
    return 1;
}

static int fsw_strcaseeq_UTF16_UTF16(void *s1data, void *s2data, int len)
{
    int i;
    fsw_u16 *p1 = (fsw_u16 *)s1data;
    fsw_u16 *p2 = (fsw_u16 *)s2data;
    fsw_u32 c1, c2;

    for (i = 0; i < len; i++) {
        c1 = ucs2_toupper(*p1++);
        c2 = ucs2_toupper(*p2++);
        if (c1 != c2)
            return 0;
    }
    return 1;
}

static int fsw_strcaseeq_ISO88591_ISO88591(void *s1data, void *s2data, int len)
{
    if (strncasecmp(s1data, s2data, len)) {
        return 0;
    }
    return 1;
}

static fsw_status_t fsw_strcoerce_UTF16_ISO88591(void *srcdata, int srclen, struct fsw_string *dest)
{
    fsw_status_t    status;
    int             i;
    fsw_u16       *sp;
    fsw_u8       *dp;
    fsw_u32         c;

    dest->type = FSW_STRING_TYPE_ISO88591;
    dest->len  = srclen;
    dest->size = srclen * sizeof(fsw_u8);
    status = fsw_alloc(dest->size, &dest->data);
    if (status)
        return status;

    sp = (fsw_u16 *)srcdata;
    dp = (fsw_u8 *)dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++;
        *dp++ = (fsw_u8)c;
    }
    return FSW_SUCCESS;
}

static fsw_status_t fsw_strcoerce_ISO88591_UTF16(void *srcdata, int srclen, struct fsw_string *dest)
{
    fsw_status_t    status;
    int             i;
    fsw_u8       *sp;
    fsw_u16       *dp;
    fsw_u32         c;

    dest->type = FSW_STRING_TYPE_UTF16;
    dest->len  = srclen;
    dest->size = srclen * sizeof(fsw_u16);
    status = fsw_alloc(dest->size, &dest->data);
    if (status)
        return status;

    sp = (fsw_u8 *)srcdata;
    dp = (fsw_u16 *)dest->data;
    for (i = 0; i < srclen; i++) {
        c = *sp++;
        *dp++ = (fsw_u16)c;
    }
    return FSW_SUCCESS;
}
