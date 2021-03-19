/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * EFI_CRYPTO_MBEDTLS_PROTOCOL
 *
 * This protocol exports a small subset of mbedTLS functionality, allowing
 * crypto code to be isolated in a separate .efi driver module that can
 * eventually be FIPS certified.
 */

#ifndef __EFI_MBEDTLS_PROTOCOL_H__
#define __EFI_MBEDTLS_PROTOCOL_H__

/* fccaf641-5030-4348-8c0d-82699e8491ac */
#define VMW_MBEDTLS_PROTOCOL_GUID { \
   0xfccaf641, 0x5030, 0x4348, \
      { 0x8c, 0x0d, 0x82, 0x69, 0x9e, 0x84, 0x91, 0xac } \
}

#define MBEDTLS_CURRENT_VERSION 4

typedef struct _EFI_MBEDTLS_PROTOCOL EFI_MBEDTLS_PROTOCOL;


/*
 * Initialize an RSA context.
 */
typedef
void
(*MBEDTLS_RSA_INIT)(
    mbedtls_rsa_context *ctx,
    int padding,
    int hash_id);

/*
 * Do an RSA operation and check the message digest.
 */
typedef
int
(*MBEDTLS_RSA_PKCS1_VERIFY)(
    mbedtls_rsa_context *ctx,
    int (*f_rng)(void *, unsigned char *, size_t),
    void *p_rng,
    int mode,
    mbedtls_md_type_t md_alg,
    unsigned int hashlen,
    const unsigned char *hash,
    const unsigned char *sig
);

/*
 * Set a multiple precision integer from an integer.
 */
typedef
int
(*MBEDTLS_MPI_LSET)(
    mbedtls_mpi *X,
    mbedtls_mpi_sint z);

/*
 * Set a multiple precision integer from unsigned big endian binary data.
 */
typedef
int
(*MBEDTLS_MPI_READ_BINARY)(
    mbedtls_mpi *X,
    const unsigned char *buf,
    size_t buflen);

/*
 * Set a multiple precision integer from an ASCII string.
 */
typedef
int
(*MBEDTLS_MPI_READ_STRING)(
    mbedtls_mpi *X,
    int radix,
    const char *s);

/*
 * output = SHA-256(input buffer)
 */
typedef
int
(*MBEDTLS_SHA256_RET)(
    const unsigned char *input,
    size_t ilen,
    unsigned char output[32],
    int is224
);

/*
 * output = SHA-512(input buffer)
 */
typedef
int
(*MBEDTLS_SHA512_RET)(
    const unsigned char *input,
    size_t ilen,
    unsigned char output[64],
    int is384
);

/*
 * output = HMAC(hmac key, input buffer)
 */
typedef
int
(*MBEDTLS_HMAC_RET)(
    const mbedtls_md_type_t md_type,
    const unsigned char *key,
    size_t keylen,
    const unsigned char *input,
    size_t ilen,
    unsigned char *output
);

struct _EFI_MBEDTLS_PROTOCOL {
   UINT32 Version;
   MBEDTLS_RSA_INIT RsaInit;
   MBEDTLS_RSA_PKCS1_VERIFY RsaPkcs1Verify;
   MBEDTLS_MPI_LSET MpiLset;
   MBEDTLS_MPI_READ_BINARY MpiReadBinary;
   MBEDTLS_MPI_READ_STRING MpiReadString;
   MBEDTLS_SHA256_RET Sha256Ret;
   MBEDTLS_SHA512_RET Sha512Ret;
   MBEDTLS_HMAC_RET HmacRet;
};

#endif
