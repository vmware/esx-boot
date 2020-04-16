/*******************************************************************************
 * Copyright (c) 2015,2017-2018 VMware, Inc.  All rights reserved.
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
 */

#include "mboot.h"
#include "boot_services.h"
#include "libgen.h"

#include <sha256.h>
#include <sha512.h>
#include <rsa.h>

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

typedef struct {
   const char *name;
   unsigned found;
} NamedModule;

/*
 * List of early modules that are identified by name.  Only the basename with
 * extension removed is significant.  Each is required to be signed if present
 * and may be present only once.  Any changes to the list require moving to a
 * new schema version number.
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

/*
 * RSA public key.
 */
typedef struct {
   const char *keyid;
   /*
    * Raw hex form: size, modulus, and public exponent.
    * Obtainable from the command:
    *
    *    openssl rsa -pubin -text < key.public
    *
    * ...followed by removing the colons and newlines from the modulus.
    */
   unsigned bits;
   const char *modulus;
   uint64_t exponent;
   /*
    * Message digest algorithm to be used in signatures with this key.
    */
   md_type_t digest;
   /*
    * Parsed form; valid if parsed = TRUE.
    */
   bool parsed;
   rsa_context rsa;
} RawRSAKey;

RawRSAKey pubkeys[] = {
#if defined(test)
   {
      "elfsign_test",
      2048,
      "00cb5ad2434ed4f0454547fb9104404c5247b757d67bc18fa0a6184007ed"
      "0b18e1216b3a656c6e53bfe5b97442f8a59d379f9d1686bbef573a606736"
      "61066139164ace800b2350b753c5be579661424a605b9c80bf6db0721159"
      "06a03146ec283010c54eb959fce7029c101380f18216f7d3d41b949d407f"
      "fc9b549c77977df58ab7a33ba7e7c592e07cf7a656f2aa26a1bda3c9ab55"
      "cd76e34270584a7a6c6cc34a19a8c0ff5f71cd891bf5a6e18e2cd4c11390"
      "be08863face38fe1bc3b6cafd87bfdad4aabd8767cd20a0ab6a0c4a0c12f"
      "ebc1a6f974bea947f4587f007eaa7901253ee02ad71e5663bc78d6a5a142"
      "cb00429b82038dba69718dd7e2866f28cb",
      0x10001,
      POLARSSL_MD_SHA256,
      false,
      { 0 }
   },
   {
      "test_esx67",
      4096,
      "00a480237f1f4a1a4889a8ac18fff0f26caaf5d263f7b6a0425ae0725c74"
      "8fc633346cd756a5f017c78c7df39745910800f727e089f786e98e4aa79c"
      "e969344d8004bbd16cbfe08c818ecbea15709da187ac66d367c3d62a247a"
      "dfa46c1f836d75a06e9f2b0e1a172cc1a592c3459dee72cf41d859a37652"
      "ce1b800ccbe6bdd979628363c1d9a58b613ad8e0c6affb4b501f915e7182"
      "b10e8e076145cce9406efa9b856170a72b93dbd008bc409de8a8c93d127a"
      "9e352462785ff4268c2cc5323feed0e2468b5357cdd8c20dcd96a9ca3a53"
      "cf3317ea5e7233a7c4c104a8e1c93b198108cf7024ba376f29a4864b293b"
      "614358199feaf3f1121a9c45761436ed2bafce39c689a9dcd1b48a33936b"
      "f8f6e36b31d8417868b6a7b61cad1738be4fc401decd534d389e9406fa42"
      "bc4385d161a1478ef3daff6cf12f293f90631409628dddb690203acbc7fd"
      "a052aa5adec6caa0939b3564ee9dca0f077884958b112adb9ef61b753bd4"
      "efece4da83a47720f806253bd3fc0079b84f75522703cade9bffcd9d6fe0"
      "b1338d467f0b9dce609a64659fe66b0c87861cf08f7612bbb6491946a9ef"
      "02b115609c56538ec65014c70d0ffbb5dc81b0c650f1d762ecd42152f11e"
      "9159de35c64a522b5658b7c32876813e830c9a0a459c8cbf89cfe4e7eac4"
      "8319f57646fddebab57640407b567e36801deff2e86001f16a2439287d1e"
      "769301",
      0x10001,
      POLARSSL_MD_SHA512,
      false,
      { 0 }
   },
#endif
#if defined(official)
   {
      "vmware_esx40",
      2048,
      "00c565a745cf677ff3152e790e7f53bbcc762796b76afa0893b2ac847459"
      "dc3f17576de5054dfee5b9bc1d0fab80810ba4bffc64c8fe54464b5d93ff"
      "ef3088bf3c6c250aef126f46b6dc2ee6a3da71f080e5a32122fa48358b5f"
      "6629cfb91b73a28afe2952cda5e1d4d7f6f0b6178419438f8c963af44d67"
      "4b48b431139cc64c8fe48344a2500aa475b39fc9b0b90c9febbcc7c78b34"
      "2d9a25e98198a23cda2aa58fd17ff6e511143f0689141f048a1ced057f3d"
      "b9e7dd118405b03b90b89fd418050c5e5451b4de4ea5ad04f43b8b0b8546"
      "885da1941286d4f1531cabc9e4dfa26dafe09c953c75c6211879f62cb0f6"
      "badca8b35db8b47a407fe6c402b05a137f",
      0x3,
      POLARSSL_MD_SHA256,
      false,
      { 0 }
   },
   {
      "vmware_esx67",
      4096,
      "00b7cea53a8621213db4d66708969bac03778afb9a8494bf3c3c34e4b966"
      "432b7b5b5c64dd2ed0455117ea9af892f5cb76b951b9071721ea871863b1"
      "d66fa4451d9f71e90803c0d45dd8632b7ec91f40501b48aea156db5139f9"
      "bd3609ac2ed79c80d2c32475aa1514ffaa6fb5983871dafdd44efa7015c5"
      "8eb88f0e1bc9abfd6077ed9e5d48825d81f025b7f47d35f882135c1e0aa3"
      "fb8f95d3a4bf3034d66d0307047c6d2b254af4fa1a4565fedda6480bd077"
      "6ee09c759335378e1799cf70d792fd7acf1863a3ce3c0f60e392f9df3eb6"
      "d17a22135ffaabba3e8652a5541d14072c588d0f71349bf72c1e4dfdae73"
      "a31aa4431df762c825f857ba87eb3745a16c2752f41f564df9793d1e2812"
      "88a7f8450b4abca6778527112782cf51735c40f0511f88aafa573ddd1897"
      "7f562dcc895d76cfedffde4dc6c8e806a217e6d4f8896d3b5ddc664be028"
      "89486734c0a34ab1333cb38bcad422dd279dcff304f6bdf345f9f504d69c"
      "f2a8ced26426a1b09ae01d4d3608e164f6f133ee31bd96f0b0b5dc8c64d1"
      "1a0b6cfa18c4cdd1d615cb3c521b777ef812ef0122006a62e417cce68cc1"
      "b7efc7b0e948c377adde8e047e3f286072a25f54ac7bdee0e03dfdf203f0"
      "14b139bf0a12e84c31b7b6f2120b95084e16532fec8fa146381d77032975"
      "f0912c58748de0123a49a934c7c6bdabcb9339554e6d13a19c1daa41793c"
      "8d4b03",
      0x3,
      POLARSSL_MD_SHA512,
      false,
      { 0 }
   },
#endif
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
   RawRSAKey *pubkey;
   unsigned i;

   /*
    * This function works for all schema versions defined so far,
    * so the schema parameter is unused.
    */
   (void)schema;

   memcpy(keyid, sig, V1_KEYID_LEN);
   keyid[V1_KEYID_LEN] = '\0';

   for (i = 0; i < ARRAYSIZE(pubkeys); i++) {
      pubkey = &pubkeys[i];
      if (strcmp(keyid, pubkey->keyid) == 0) {
         break;
      }
   }
   if (i == ARRAYSIZE(pubkeys)) {
      Log(LOG_WARNING, "Signature has unexpected keyid %s", keyid);
      return false;
   }

   if (!pubkey->parsed) {
      Log(LOG_DEBUG, "Parsing keyid %s", pubkey->keyid);
      rsa_init(&pubkey->rsa, RSA_PKCS_V15, POLARSSL_MD_NONE);
      pubkey->rsa.len = pubkey->bits / 8;
      errcode = mpi_read_string(&pubkey->rsa.N, 16, pubkey->modulus);
      if (!errcode) {
         errcode = mpi_lset(&pubkey->rsa.E, pubkey->exponent);
      }
      if (errcode) {
         Log(LOG_WARNING, "Error parsing public key %s: -0x%x",
             pubkey->keyid, -errcode);
         return false;
      }
      pubkey->parsed = true;
   }

   if (sigLen != V1_KEYID_LEN + pubkey->rsa.len) {
      Log(LOG_WARNING, "Invalid signature length %zu, should be %zu",
          sigLen, V1_KEYID_LEN + pubkey->rsa.len);
      return false;
   }

   switch (pubkey->digest) {
   case POLARSSL_MD_SHA256:
      sha256(data, dataLen, md, 0);
      errcode = rsa_pkcs1_verify(&pubkey->rsa, NULL, NULL, RSA_PUBLIC,
                                 pubkey->digest, SHA256_DIGEST_LENGTH, md,
                                 (uint8_t*)sig + V1_KEYID_LEN);
      break;

   case POLARSSL_MD_SHA512:
      sha512(data, dataLen, md, 0);
      errcode = rsa_pkcs1_verify(&pubkey->rsa, NULL, NULL, RSA_PUBLIC,
                                 pubkey->digest, SHA512_DIGEST_LENGTH, md,
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
 *      detail about failures.  LOG_CRIT for final failure (with module name).
 *
 * Results
 *      ERR_SUCCESS: signatures are valid
 *      ERR_NOT_FOUND: boot modules are unsigned (no logging)
 *      ERR_INSECURE: signature validation failed
 *----------------------------------------------------------------------------*/
int secure_boot_check(void)
{
   int status;
   uint32_t schema0 = 0;
   unsigned i;
   unsigned errors;
   NamedModule *named;

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
      return ERR_INSECURE;
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
      return ERR_INSECURE;
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
            Log(LOG_WARNING, "More than one module named %s\n", mod->filename);
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

   return errors == 0 ? ERR_SUCCESS : ERR_INSECURE;
}

#endif
