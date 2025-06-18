/*******************************************************************************
 * Copyright (c) 2015-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#ifdef SECURE_BOOT

/*
 * secure.c -- Support for UEFI Secure Boot
 *
 *      mboot performs its phase of UEFI Secure Boot by checking signatures on
 *      all bootbank files (also called VIB payloads, or from mboot's point of
 *      view, boot modules) that are used in early boot.  We term these the
 *      "early" modules. If this check succeeds, mboot turns over control to
 *      ESXi's early boot environment. A script in the early environment will
 *      then perform the last phase of Secure Boot by checking the signatures
 *      of all VIBs and the hashes of the payloads they provide, thus covering
 *      the rest of the bootbank.
 *
 *      We define the mboot "schema" to be the set of modules that are
 *      considered early modules, including the exact algorithm for finding the
 *      early modules, together with the signature algorithm being used. Both
 *      mboot and the system being booted must agree on the schema, or mboot
 *      might check the signatures on the wrong set of modules and/or use the
 *      wrong signature verification algorithm, thus perhaps allowing secure
 *      boot to succeed when it should fail, or vice versa.
 *
 *      In all schemas, we require the signed module format to be as follows.
 *      Fields may begin on arbitrary byte boundaries and numbers are in
 *      little-endian order.
 *
 *         module data (covered by signature)
 *         [optional schema version dependent data] (covered by signature)
 *         4-byte schema version number (covered by signature)
 *         signature
 *         4-byte signature length
 *         4-byte fixed magic number = 0x1abe11ed
 *
 *      The signature length and magic number fields are at the end so that the
 *      signature can be parsed out by working backward from the end.  Those
 *      fields are not covered by the signature.  The schema version number is
 *      covered by the signature to prevent spoofing.
 *
 *      In all schemas, we require the 0th module (the multiboot "kernel") to
 *      be an early module. mboot obtains the schema number from the 0th
 *      module and checks that all other modules that the schema identifies
 *      as early have this same version number.
 *
 *      NOTE: Any future additions or changes to schema validation will also
 *            need to be made in the QuickBoot secure boot implementation.
 */

#include "mboot.h"
#include "boot_services.h"
#include "libgen.h"
#include "cert.h"

#include <efiutils.h>
#include <sha256.h>
#include <sha512.h>
#include <rsa.h>
#include <protocol/MbedTls.h>

#define SHA256_DIGEST_LENGTH (256 / 8)
#define SHA512_DIGEST_LENGTH (512 / 8)
#define MAX_DIGEST_LENGTH SHA512_DIGEST_LENGTH

#define SCHEMA_MAGIC 0x1abe11ed

/*
 * In schema versions 1-4 the first 16 bytes of a signature are an ASCII
 * key id.  This is compared with the expected key id and used to generate
 * nicer error messages on failure.
 */
#define V1_KEYID_LEN 16

static VMW_MBEDTLS_PROTOCOL *mbedtls = NULL;

static VMW_MBEDTLS_PROTOCOL InternalMbedTls = {
   MBEDTLS_CURRENT_API_VERSION,
   "Internal crypto suite",
   mbedtls_rsa_init,
   mbedtls_rsa_pkcs1_verify,
   mbedtls_mpi_lset,
   mbedtls_mpi_read_binary,
   mbedtls_mpi_read_string,
   mbedtls_sha256_ret,
   mbedtls_sha512_ret,
   /* mbedtls_hmac_ret wrapper; not used */ NULL,
};

typedef struct {
   const char *name;
   unsigned found;
} NamedModule;

/*
 * List of early modules that are identified by name.  Only the basename with
 * extension removed is significant.  Each is required to be signed if present
 * and may be present only once.  Any changes to the list require moving to a
 * new schema version number.
 *
 * NOTE! Any schema change requires QuickBoot changes, too!
 */
NamedModule v1Named[] = {
   {"s",  0},
   {"sb", 0},
   {NULL, 0}
};

NamedModule v2Named[] = { // also correct for v3
   {"s",  0},
   {"sb", 0},
   {"esxcore", 0}, // present only in esxcore builds
   {NULL, 0}
};

NamedModule v4Named[] = {
   {"s",  0},
   {"sb", 0},
   {"esxcore", 0}, // present only in esxcore builds
   {"esxupdt", 0}, // esximage library
   {NULL, 0}
};


/*-- secure_boot_parse_module --------------------------------------------------
 *
 *      Parse out the data and signature fields of a signed module.
 *
 * Parameters
 *      IN addr:        address of module
 *      IN len:         length of module
 *      OUT schemaOut:  schema version number of module's signature
 *      OUT dataOut:    address of signed data
 *      OUT dataLenOut: length of signed data
 *      OUT sigOut:     address of signature
 *      OUT sigLenOut:  length of signature
 *
 * Results
 *      ERR_SUCCESS:   success
 *      ERR_NOT_FOUND: module has no signature
 *      ERR_SYNTAX:    module format is invalid
 *
 *      On success, returns
 *      field sizes and pointers to the fields.  All OUT parameters may be NULL
 *      if the value is not needed.
 *----------------------------------------------------------------------------*/
static int secure_boot_parse_module(void *addr,           //IN
                                    size_t len,           //IN
                                    uint32_t *schemaOut,  //OUT
                                    void **dataOut,       //OUT
                                    size_t *dataLenOut,   //OUT
                                    void **sigOut,        //OUT
                                    size_t *sigLenOut)    //OUT
{
   uint8_t *p;
   uint32_t magic, sigLen, schema;

   p = (uint8_t *)addr + len;
   p -= sizeof(magic);
   if (p < (uint8_t *)addr) {
      return ERR_NOT_FOUND;
   }
   memcpy(&magic, p, sizeof(magic));
   if (magic != SCHEMA_MAGIC) {
      return ERR_NOT_FOUND;
   }

   p -= sizeof(sigLen);
   if (p < (uint8_t *)addr) {
      return ERR_SYNTAX;
   }
   memcpy(&sigLen, p, sizeof(sigLen));

   p -= sigLen;
   if (p < (uint8_t *)addr) {
      return ERR_SYNTAX;
   }
   if (sigOut != NULL) {
      *sigOut = p;
   }
   if (sigLenOut != NULL) {
      *sigLenOut = sigLen;
   }
   if (dataOut != NULL) {
      *dataOut = addr;
   }
   if (dataLenOut != NULL) {
      *dataLenOut = p - (uint8_t *)addr;
   }

   p -= sizeof(schema);
   if (p < (uint8_t *)addr) {
      return ERR_SYNTAX;
   }
   memcpy(&schema, p, sizeof(schema));
   if (schemaOut != NULL) {
      *schemaOut = schema;
   }

   return ERR_SUCCESS;
}


/*-- find_named_module ---------------------------------------------------------
 *
 *      Look for the basename (stripping directory name and extension) of the
 *      given name in a NamedModule list.  If found, increment its count.
 *
 * Parameters
 *      IN name:        module name
 *      IN list:        list of known names
 *
 * Results
 *      ERR_SUCCESS:         name is in the list and its count is now 1
 *      ERR_NOT_FOUND:       name is not in the list (not an error)
 *      ERR_ALREADY_STARTED: name is in the list and its count is now >1
 *----------------------------------------------------------------------------*/
int find_named_module(char *name, NamedModule *list)
{
   char *slash;
   char *dot;
   char *bn;
   int len;

   slash = strrchr(name, '/');
   if (slash != NULL) {
      bn = slash + 1;
   } else {
      bn = name;
   }
   dot = strrchr(bn, '.');
   if (dot != NULL) {
      len = dot - bn;
   } else {
      len = strlen(bn);
   }

   while (list->name != NULL) {
      if (strncmp(bn, list->name, len) == 0 && list->name[len] == '\0') {
         list->found++;
         if (list->found > 1) {
            return ERR_ALREADY_STARTED;
         } else {
            return ERR_SUCCESS;
         }
      }
      list++;
   }
   return ERR_NOT_FOUND;
}


/*-- secure_boot_check_sig -----------------------------------------------------
 *
 *      Check one attached signature
 *
 * Parameters
 *      IN schema:  schema version number (determines signature algorithm)
 *      IN data:    signed data
 *      IN dataLen: length of data in bytes
 *      IN sig:     signature
 *      IN sigLen   length of signature in bytes
 *
 * Results
 *      true if signature checks out; false if not.
 *----------------------------------------------------------------------------*/
static bool secure_boot_check_sig(uint32_t schema,
                                  void *data, size_t dataLen,
                                  void *sig, size_t sigLen)
{
   unsigned char md[MAX_DIGEST_LENGTH];
   int errcode;
   char keyid[V1_KEYID_LEN + 1];
   RawRSACert *cert;

   /*
    * This function works for all schema versions defined so far,
    * so the schema parameter is unused.
    */
   (void)schema;

   memcpy(keyid, sig, V1_KEYID_LEN);
   keyid[V1_KEYID_LEN] = '\0';

   for (cert = certs; ; ++cert) {
      if (cert->keyid == NULL) {
         Log(LOG_WARNING, "Signature has unexpected keyid %s", keyid);
         return false;
      }
      if (strcmp(keyid, cert->keyid) == 0) {
         break;
      }
   }

   if (!cert->parsed) {
      Log(LOG_DEBUG, "Parsing keyid %s", cert->keyid);
      mbedtls->RsaInit(&cert->rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
      /*
       * Modulus has always MSB bit set.  To ensure it is not treated as
       * negative number, zero byte is prepended - so modulus for RSA 2048
       * actually has 2056 bits - 8 zero bits, 1 one bit, 2046 variable
       * bits, and 1 one bit.
       *
       * So to get key length in bytes we must subtract one from modulus
       * length in bytes.
       */
      cert->rsa.len = cert->modulusLength - 1;
      errcode = mbedtls->MpiReadBinary(&cert->rsa.N,
                                       cert->certData + cert->modulusStart,
                                       cert->modulusLength);
      if (!errcode) {
         errcode = mbedtls->MpiReadBinary(&cert->rsa.E,
                                          cert->certData + cert->exponentStart,
                                          cert->exponentLength);
      }
      if (errcode) {
         Log(LOG_WARNING, "Error parsing public key %s: -0x%x",
             cert->keyid, -errcode);
         return false;
      }
      cert->parsed = true;
   }

   if (sigLen != V1_KEYID_LEN + cert->rsa.len) {
      Log(LOG_WARNING, "Invalid signature length %zu, should be %zu",
          sigLen, V1_KEYID_LEN + cert->rsa.len);
      return false;
   }

   if (!cert->measured && boot.tpm_measure) {
      errcode = tpm_extend_signer(cert->certData, cert->certLength);
      if (errcode != ERR_SUCCESS) {
         Log(LOG_ERR, "Failed to log certificate %s: %s", cert->keyid,
             error_str[errcode]);
         return false;
      } else {
         cert->measured = TRUE;
      }
   }

   switch (cert->digest) {
   case MBEDTLS_MD_SHA256:
      mbedtls->Sha256Ret(data, dataLen, md, 0);
      errcode = mbedtls->RsaPkcs1Verify(&cert->rsa, NULL, NULL,
                                        MBEDTLS_RSA_PUBLIC, cert->digest,
                                        SHA256_DIGEST_LENGTH, md,
                                        (uint8_t*)sig + V1_KEYID_LEN);
      break;

   case MBEDTLS_MD_SHA512:
      mbedtls->Sha512Ret(data, dataLen, md, 0);
      errcode = mbedtls->RsaPkcs1Verify(&cert->rsa, NULL, NULL,
                                        MBEDTLS_RSA_PUBLIC, cert->digest,
                                        SHA512_DIGEST_LENGTH, md,
                                        (uint8_t*)sig + V1_KEYID_LEN);
      break;

   default:
      NOT_REACHED();
   }

   if (errcode) {
      Log(LOG_WARNING, "Error verifying signature: -0x%x", -errcode);
      return false;
   }

   return true;
}


/*-- secure_boot_check ---------------------------------------------------------
 *
 *      Determine the schema version in use, find the early modules, and check
 *      their signatures.
 *
 *      Logging strategy: LOG_DEBUG for non-error messages.  LOG_WARNING for
 *      detail about failures.  LOG_CRIT for security violation.
 *
 * Parameters
 *      IN crypto_module: use external crypto module
 *
 * Results
 *      ERR_SUCCESS: signatures are valid
 *      ERR_NOT_FOUND: boot modules are unsigned (no logging)
 *      ERR_SECURITY_VIOLATION: signature validation failed
 *      ERR_LOAD_ERROR: crypto not available
 *----------------------------------------------------------------------------*/
int secure_boot_check(bool crypto_module)
{
   int status;
   uint32_t schema0 = 0;
   unsigned i;
   unsigned errors;
   NamedModule *named;

   if (crypto_module) {
#ifdef CRYPTO_MODULE
      EFI_STATUS Status;
      EFI_GUID MbedTlsProto = VMW_MBEDTLS_PROTOCOL_GUID;
      Status = LocateProtocol(&MbedTlsProto, (void **)&mbedtls);
      if (EFI_ERROR(Status)) {
         Log(LOG_WARNING, "Error locating crypto module API: %s",
             error_str[error_efi_to_generic(Status)]);
         return ERR_LOAD_ERROR;
      }
      Log(LOG_INFO, "Located crypto module: %s", mbedtls->ModuleVersion);
      if (mbedtls->ApiVersion != MBEDTLS_CURRENT_API_VERSION) {
         Log(LOG_WARNING, "Incorrect crypto module API version: %u",
             mbedtls->ApiVersion);
         return ERR_LOAD_ERROR;
      }
#else
      return ERR_LOAD_ERROR;
#endif
   } else {
      mbedtls = &InternalMbedTls;
   }

   status = secure_boot_parse_module(boot.modules[0].addr,
                                     boot.modules[0].size,
                                     &schema0, NULL, NULL, NULL, NULL);
   switch (status) {
   case ERR_SUCCESS:
      break;
   case ERR_NOT_FOUND:
      // Boot modules are not signed
      return ERR_NOT_FOUND;
   case ERR_SYNTAX:
      Log(LOG_CRIT, "Invalid attached signature format on module 0 (%s)",
          boot.modules[0].filename);
      return ERR_SECURITY_VIOLATION;
   default:
      NOT_REACHED();
   }

   switch (schema0) {
   case 1:
      named = v1Named;
      break;
   case 2:
   case 3:
      named = v2Named;
      break;
   case 4:
      named = v4Named;
      break;
   default:
      Log(LOG_CRIT, "Unknown schema version %u on module 0 (%s)",
          schema0, boot.modules[0].filename);
      return ERR_SECURITY_VIOLATION;
   }

   /*
    * In schema versions 1-4:
    * - All ELF modules must be signed.
    * - The modules listed by name for the schema must be signed and
    *   their names must not be duplicated.
    */
   errors = 0;
   for (i = 0; i < boot.modules_nr; i++) {
      module_t *mod = &boot.modules[i];
      bool needsig = false;
      bool ok;
      void *data = NULL;
      size_t dataLen = -1;
      void *sig = NULL;
      size_t sigLen = -1;
      uint32_t schema = -1;

      if (mod->size >= SELFMAG &&
          memcmp(ELFMAG, mod->addr, SELFMAG) == 0) {

         needsig = true;

      } else {
         switch (find_named_module(mod->filename, named)) {
         case ERR_SUCCESS:
            needsig = true;
            break;
         case ERR_NOT_FOUND:
            needsig = false;
            break;
         case ERR_ALREADY_STARTED:
            Log(LOG_WARNING, "More than one module named %s", mod->filename);
            errors++;
            needsig = true;
            break;
         default:
            NOT_REACHED();
         }
      }

      if (!needsig) {
         continue;
      }

      ok = false;
      status = secure_boot_parse_module(mod->addr, mod->size,
                                        &schema, &data, &dataLen,
                                        &sig, &sigLen);
      switch (status) {
      case ERR_NOT_FOUND:
         Log(LOG_WARNING, "No signature found");
         break;
      case ERR_SYNTAX:
         Log(LOG_WARNING, "Invalid attached signature format");
         break;
      case ERR_SUCCESS:
         if (schema != schema0) {
            Log(LOG_WARNING, "Wrong schema version (got %u; expected %u)",
                schema, schema0);
         } else {
            ok = secure_boot_check_sig(schema, data, dataLen, sig, sigLen);
         }
         break;
      default:
         NOT_REACHED();
      }

      Log(ok ? LOG_DEBUG : LOG_CRIT, "Signature check %s on module %u (%s)",
          ok ? "succeeded" : "failed", i, mod->filename);

      if (!ok) {
         errors++;
      }
   }

   return errors == 0 ? ERR_SUCCESS : ERR_SECURITY_VIOLATION;
}

#endif
