/*******************************************************************************
 * Copyright (c) 2020-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 *  crypto.h -- Internal header for the crypto module
 */

#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <md.h>
#include <rsa.h>
#include <sha256.h>
#include <sha512.h>

#include <protocol/MbedTls.h>

extern VMW_MBEDTLS_PROTOCOL *mbedtls;

void failure(const char *msg);
void self_test(void);
void integrity_test(void);

#endif /* !CRYPTO_H_ */
