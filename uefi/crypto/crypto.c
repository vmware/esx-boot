/*******************************************************************************
 * Copyright (c) 2020-2021 VMware, Inc.  All rights reserved.
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
#include <stack_chk.h>
#include <efi_private.h>

#include "crypto.h"

EFI_BOOT_SERVICES *bs = NULL;
EFI_RUNTIME_SERVICES *rs = NULL;
EFI_SYSTEM_TABLE *st = NULL;
EFI_HANDLE ImageHandle;

static EFI_GUID MbedTlsProto = VMW_MBEDTLS_PROTOCOL_GUID;

/*-- fips_hmac -----------------------------------------------------------------
 *
 *      Wrapper for mbedtls_md_hmac.  Enforces FIPS lower bound on key length
 *      (112 bits) and converts md_type to mbedtls_md_info.
 *------------------------------------------------------------------------------
 */
static int fips_hmac(const mbedtls_md_type_t md_type,
                     const unsigned char *key, size_t keylen,
                     const unsigned char *input, size_t ilen,
                     unsigned char *output)
{
   if (keylen * 8 < 112) {
      return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
   }
   return mbedtls_md_hmac(mbedtls_md_info_from_type(md_type), key, keylen,
                          input, ilen, output);
}

/*-- fips_rsa_pkcs1_verify -----------------------------------------------------
 *
 *      Wrapper for mbedtls_rsa_pkcs1_verify.  Enforces FIPS lower bound on key
 *      length (1024 bits).
 *------------------------------------------------------------------------------
 */
static int fips_rsa_pkcs1_verify(mbedtls_rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng, int mode,
                                 mbedtls_md_type_t md_alg,
                                 unsigned int hashlen,
                                 const unsigned char *hash,
                                 const unsigned char *sig)
{
   if (ctx->len * 8 < 1024) {
      return MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
   }
   return mbedtls_rsa_pkcs1_verify(ctx, f_rng, p_rng, mode, md_alg,
                                   hashlen, hash, sig);
}

/*
 * Interface structure for the MbedTLS crypto protocol.  Most functions come
 * directly from mbedtls, but two require wrappers to enforce FIPS lower
 * bounds on key length.
 */
static VMW_MBEDTLS_PROTOCOL MbedTls = {
    MBEDTLS_CURRENT_API_VERSION,
    "VMware's ESXboot Cryptographic Module, v1.0",
    mbedtls_rsa_init,
    fips_rsa_pkcs1_verify,
    mbedtls_mpi_lset,
    mbedtls_mpi_read_binary,
    mbedtls_mpi_read_string,
    mbedtls_sha256_ret,
    mbedtls_sha512_ret,
    fips_hmac
};

VMW_MBEDTLS_PROTOCOL *mbedtls = &MbedTls;


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

/*-- __stack_chk_fail ----------------------------------------------------------
 *
 *      Compiler-generated stack smash checking code calls this function on
 *      failure.
 *----------------------------------------------------------------------------*/
void __stack_chk_fail(void)
{
   failure("Fatal error: Stack smash detected");
}

/*-- main ----------------------------------------------------------------------
 *
 *      Image entry point.
 *----------------------------------------------------------------------------*/

EFI_STATUS EFIAPI efi_main(EFI_HANDLE Handle, EFI_SYSTEM_TABLE *SystemTable)
{
   EFI_STATUS Status;
   EFI_LOADED_IMAGE *Image;

   ImageHandle = Handle;
   st = SystemTable;
   bs = st->BootServices;
   rs = st->RuntimeServices;
   __stack_chk_init();

   Status = image_get_info(Handle, &Image);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   mem_init(Image->ImageDataType);

   self_test();
   integrity_test();

   Status = bs->InstallProtocolInterface(&ImageHandle,
                                         &MbedTlsProto,
                                         EFI_NATIVE_INTERFACE,
                                         &MbedTls);

   bs->Exit(ImageHandle, Status, 0, NULL); // does not return

   return EFI_ABORTED; // not actually reachable
}
