/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 *  crypto.c --
 *
 *      Driver module that exports crypto functions from mbedtls as a UEFI
 *      protocol. The driver is intended to eventually be FIPS certified.
 */

#include <sys/stat.h>
#include <efiutils.h>
#include <bootlib.h>
#include <boot_services.h>

#include "crypto.h"

static EFI_GUID MbedTlsProto = VMW_MBEDTLS_PROTOCOL_GUID;

/*---- mbedtls_hmac_ret --------------------------------------------------------
 *
 *    This function calculates the full generic HMAC on the input buffer with
 *    the provided key.
 *    The function allocates the context, performs the calculation, and frees
 *    the context.
 *
 *    The HMAC result is calculated as
 *    output = generic HMAC(hmac key, input buffer).
 *
 * Parameters
 *    IN md_type:  The digest identifier
 *    IN key:      The HMAC secret key.
 *    IN keylen:   The length of the HMAC secret key in Bytes.
 *    IN input:    The buffer holding the input data.
 *    IN ilen:     The length of the input data.
 *    OUT output   The generic HMAC result.
 *
 * Results
 *    0 on success,  #MBEDTLS_ERR_MD_BAD_INPUT_DATA on parameter-verification
 *                   failure.
 *------------------------------------------------------------------------------
 */
static int mbedtls_hmac_ret(const mbedtls_md_type_t md_type,
                            const unsigned char *key, size_t keylen,
                            const unsigned char *input, size_t ilen,
                            unsigned char *output)
{
   return mbedtls_md_hmac(mbedtls_md_info_from_type(md_type), key, keylen,
                          input, ilen, output);
}

/*
 * Interface structure for the MbedTLS protocol.
 */
static EFI_MBEDTLS_PROTOCOL MbedTls = {
    MBEDTLS_CURRENT_VERSION,
    mbedtls_rsa_init,
    mbedtls_rsa_pkcs1_verify,
    mbedtls_mpi_lset,
    mbedtls_mpi_read_binary,
    mbedtls_mpi_read_string,
    mbedtls_sha256_ret,
    mbedtls_sha512_ret,
    mbedtls_hmac_ret
};

EFI_MBEDTLS_PROTOCOL *mbedtls = &MbedTls;


/*-- failure -------------------------------------------------------------------
 *
 *      Power-on test failure.  Exit with a message in ExitData.  If mboot is
 *      the caller, it will log the message at LOG_DEBUG level.
 *
 * Parameters
 *      IN Status:  EFI error status to return
 *      IN msg:     message to return (as ExitData)
 *
 * Results
 *      Does not return.
 *----------------------------------------------------------------------------*/
void failure(const char *msg)
{
   CHAR16 *Msg = NULL;
   unsigned size = 0;

   if (msg != NULL) {
      ascii_to_ucs2(msg, &Msg);
   }
   if (Msg != NULL) {
      size = (ucs2_strlen(Msg) + 1) * 2;
   }

   bs->Exit(ImageHandle, EFI_SECURITY_VIOLATION, size, Msg);
}

/*-- main ----------------------------------------------------------------------
 *
 *      Image entry point.
 *----------------------------------------------------------------------------*/

int main(UNUSED_PARAM(int argc), UNUSED_PARAM(char **argv))
{
    EFI_STATUS Status;

    integrity_test();
    self_test();

    Status = bs->InstallProtocolInterface(&ImageHandle,
                                          &MbedTlsProto,
                                          EFI_NATIVE_INTERFACE,
                                          &MbedTls);
    return Status;
}
