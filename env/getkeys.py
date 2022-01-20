#! /usr/bin/python

#*******************************************************************************
# Copyright (c) 2020-2022 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

# Extract public key from an authenticode certificate to C source.
# Usage: getkeys.py < input_file.json > output_file.c
#
# Input json file has format:
# {
#    "test": ["key1", "key2", ...],
#    "official": ["key1", "key2", ...]
# }

import base64
import json
import os
import re
import subprocess
import sys
import time

# Load keyinfo library
keyinfo_dir = os.getenv('KEYINFO_DIR', None)
if keyinfo_dir:
    sys.path.append(keyinfo_dir)
    from lib import keyinfo
    get_pem = keyinfo.GetPem
else:
    def get_pem(key):
        return "../localkeys/%s.pem" % key

hash_algs = dict(
    sha1WithRSAEncryption='SHA256',
    sha256WithRSAEncryption='SHA256',
    sha512WithRSAEncryption='SHA512'
)

class asn1line:
    def __init__(self, line):
        # Parse lines like
        #  123:d=2 hl=2 l=111 prim: INTEGER        :1234
        #  234:d=9 hl=1 l=  1 cons: foo [ 3 ]
        m = re.match(r'\s*(\d+):d=\s*(\d+)\s+hl=\s*(\d+)\s+l=\s*(\d+)\s+'
                      '(\S+):\s+(.{1,18})\s*:?(.*)', line)
        if m:
            self.offset = int(m.group(1))
            self.d = int(m.group(2))
            self.hl = int(m.group(3))
            self.l = int(m.group(4))
            self.prim = m.group(5)
            self.op = m.group(6).strip()
            self.param = m.group(7).strip()
        else:
            raise ValueError("Unexpected asn1parse output %s" % line)


def parse_der(data):
    proc = subprocess.Popen([os.getenv("HOST_OPENSSL", "openssl"),
                             "asn1parse", "-inform", "der"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    (out, err) = proc.communicate(data)
    if proc.returncode != 0:
        raise ValueError("asn1parse failed with error code %d" %
                         proc.returncode)
    return [asn1line(line) for line in out.decode('utf-8').splitlines()]


def get_cert(pem_file):
    """\
    Retrieve DER-encoded certificate from PEM file.
    """
    with open(pem_file, 'rt') as f:
        mode = 0
        crt = []
        for line in f:
            if line.startswith('-----'):
                if mode != 0 or line.startswith('-----BEGIN CERTIFICATE-----'):
                    mode += 1
                continue
            if mode == 1:
                crt.append(line.rstrip())
    if mode < 2:
         raise ValueError("Certificate was not found in %s" % pem_file)
    return bytearray(base64.b64decode(''.join(crt)))


def process_key(output, key):
    """\
    Extract a public key from a certificate, and format it as
    RawRSAKey structure.
    """
    pem_file = get_pem(key)
    if pem_file is None:
        raise ValueError("Key %s does not exist" % key)
    der = get_cert(pem_file)

    args = {
        'key_name': key,
    }

    der_info = parse_der(der)

    sigalg = None
    last_object = None
    key_loc = None
    for tag in der_info:
        if tag.op == 'OBJECT':
            if sigalg is None:
                sigalg = tag.param
                if tag.d != 3:
                    raise ValueError('Unexpected OID found')
                continue
            last_object = tag
            continue
        if tag.op == 'BIT STRING':
            if last_object is None or tag.d + 1 != last_object.d:
                raise ValueError('Found bit string without associated OID')
            if last_object.param != 'rsaEncryption':
                raise ValueError('Key is not RSA key')
            key_loc = (tag.offset + tag.hl + 1, tag.l - 1)
            break
    if sigalg is None:
        raise ValueError('Signature algorithm is not set')

    # It is not necessarily true that certificate's signature algorithm
    # matches signing algorithm used by signing, but it holds true for
    # our use case.  Ideally algorithm should have been saved together
    # with key name as part of the signature.
    try:
        args['hash'] = hash_algs[sigalg]
    except KeyError:
        raise ValueError('Unknown signature algorithm %s' % sigalg)

    if key_loc is None:
        raise ValueError('RSA public key not present in certificate')

    key_der = der[key_loc[0]:key_loc[0] + key_loc[1]]

    key_info = parse_der(key_der)

    modulus_offset = None
    modulus_length = None
    exponent_offset = None
    exponent_length = None
    for tag in key_info:
        if tag.op == 'INTEGER':
            if modulus_offset is None:
                modulus_offset = key_loc[0] + tag.offset + tag.hl
                modulus_length = tag.l
            else:
                exponent_offset = key_loc[0] + tag.offset + tag.hl
                exponent_length = tag.l
                break;
    if exponent_offset is None:
        raise ValueError('RSA key does not contain modulus and exponent')

    if modulus_length < 2 or \
       der[modulus_offset] != 0 or \
       der[modulus_offset + 1] < 128:
        raise ValueError('RSA modulus does not have %s valid bits' %
                         (modulus_length - 1) * 8)

    args['certLength'] = len(der)
    args['modulusStart'] = modulus_offset
    args['modulusLength'] = modulus_length
    args['exponentStart'] = exponent_offset
    args['exponentLength'] = exponent_length

    cert_rows = []
    for x in range(0, len(der), 16):
        cert_rows.append('      \"%s\"' %
                         ''.join(["\\x%02x" % x
                                  for x in der[x:x+16]]))
    args['certData'] = '\n'.join(cert_rows)

    output.write("""\
   {
      "%(key_name)s",
      (const unsigned char *)
%(certData)s,
      %(certLength)s,
      %(modulusStart)s, %(modulusLength)s,
      %(exponentStart)s, %(exponentLength)s,
      MBEDTLS_MD_%(hash)s,
      false,
      false,
      { 0 }
   },
""" % args)


def main(inputFile, output):
    signinfo = json.load(inputFile)
    if len(sys.argv) > 1 and sys.argv[1] == '--list':
        for keys in signinfo.values():
            for key in keys:
                print("%s" % get_pem(key))
        sys.exit(0)

    args = dict(cmd=sys.argv[0], year=time.strftime('%Y'))
    output.write('''\
/*******************************************************************************
 * Copyright (c) %(year)s VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * DO NOT EDIT.  This file was automatically generated by
 * %(cmd)s
 */

#ifdef SECURE_BOOT

#include "cert.h"

RawRSACert certs[] = {
''' % args)
    # Sort reverse, so 'test' is before 'official', like in original code
    for condition, keys in sorted(signinfo.items(), reverse=True):
        if condition:
            output.write('#if defined(%s)\n' % condition)
        for key in keys:
            process_key(output, key)
        if condition:
            output.write('#endif /* defined(%s) */\n' % condition)
    output.write('''\
   {
      NULL,
      NULL, 0,
      0, 0,
      0, 0,
      MBEDTLS_MD_NONE,
      false,
      false,
      { 0 }
   }
};

#endif /* SECURE_BOOT */
''')
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.stdin, sys.stdout))
