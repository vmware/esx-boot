/*******************************************************************************
 * Copyright (c) 2015-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#pragma once

/*
 * cert.h -- Certificates for secure boot validation
 */

#include <rsa.h>
#include <stdbool.h>

/*
 * RSA public key information.
 */
typedef struct {
   const char *keyid;
   const unsigned char *certData;
   uint16_t certLength;
   /*
    * Modulus and exponent location and size in certData.
    */
   uint16_t modulusStart;
   uint16_t modulusLength;
   uint16_t exponentStart;
   uint16_t exponentLength;
   /*
    * Message digest algorithm to be used in signatures with this key.
    */
   mbedtls_md_type_t digest;
   /*
    * Parsed form; valid if parsed = TRUE.
    */
   bool parsed;
   /*
    * Measured into the TPM once before use.
    */
   bool measured;
   mbedtls_rsa_context rsa;
} RawRSACert;

extern RawRSACert certs[];
