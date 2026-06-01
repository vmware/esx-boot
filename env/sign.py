#! /usr/bin/python

#*******************************************************************************
# Copyright (c) 2015-2022 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

# Wrapper script around the signing process.
# Usage: sign.py key1+key2+...+keyn inputFile [outputFile]
# Environment variables used:
#   GOBUILD_OFFICIAL_SIGNING_ALLOWED, SBSIGN, SBATTACH, SBVERIFY, SIGNC,
#   TOPDIR, LOCALKEYS

import sys
import os
import subprocess

# Pick up optional environment variables.
official = os.getenv('GOBUILD_OFFICIAL_SIGNING_ALLOWED') == '1'
sbsign = os.getenv('SBSIGN') or 'sbsign'
sbattach = os.getenv('SBATTACH') or 'sbattach'
sbverify = os.getenv('SBVERIFY') or 'sbverify'
signc = os.getenv('SIGNC') or 'signc'
topdir = os.getenv('TOPDIR') or '.'
sigcache = topdir + '/sigcache'
localkeys = os.getenv('LOCALKEYS') or topdir + '/localkeys'
uefi_ca_cert = {
   'uefi_ca2011': { 'app': 'MicCorUEFCA2011_2011-06-27.pem',
                    'dvr': 'MicCorUEFCA2011_2011-06-27.pem' },
   'uefi_ca2023': { 'app': 'microsoft_uefi_ca_2023.pem',
                    'dvr': 'microsoft_option_rom_uefi_ca_2023.pem' }
}
sigcache_subdir = {
   'uefi_ca2011': { 'app': 'SignedBy_MSFT2011UEFICA',
                    'dvr': 'SignedBy_MSFT2011UEFICA' },
   'uefi_ca2023': { 'app': 'SignedBy_MSFT2023UEFICA',
                    'dvr': 'SignedBy_OROM2023UEFICA' }
}

dvrs = ('iso9660', 'crypto')

# Determine whether file is an app or driver
def kind(iname):
   for dvr in dvrs:
      if dvr in iname:
         return 'dvr'
   return 'app'

# Construct filename for a detached signature.
def signame(iname, key):
   return iname + '@' + key + '.tmp'

# Sign locally with sbsigntool.  Usable only with test keys, since it
# requires access to the private key as a file.  Creates detached
# signature file signame(iname, key).
def localsign(iname, key):
   k = localkeys + '/' + key + '/' + key
   subprocess.check_call([sbsign,
                          '--key', k + '.key',
                          '--cert', k + '.cert',
                          '--detached',
                          '--output', signame(iname, key),
                          iname])

# Sign remotely with signc.  Usable with official keys.
# Creates detached signature file signame(iname, key).
def remotesign(iname, key):
   tmpname = iname + '-' + key + '.tmp'
   subprocess.check_call([signc,
                          '--verbose',
                          '--signmethod', 'winddk-8.1',
                          '--hash', 'sha256',
                          '--key', key,
                          '--input', iname,
                          '--output', tmpname])
   ret = subprocess.check_call([sbattach,
                                '--detach', signame(iname, key), tmpname])
   os.remove(tmpname)

# Obtain the cached UEFI-CA signature.
def cachedsign(iname, key):
   cname = sigcache + '/' + sigcache_subdir[key][kind(iname)] + \
      '/' + os.path.basename(iname)
   sname = signame(iname, key)

   # Check that a cached signature exists.  If not, quietly skip
   # making this signed target.  Needed because the Makefiles are not
   # smart enough to skip trying to sign test binaries that we haven't
   # asked UEFI-CA to sign.
   if not os.path.isfile(cname):
      print('Skipped signing with %s; no cached signature' % key)
      exit(0)

   subprocess.check_call([sbattach, '--detach', sname, cname])

   # Verify that the cached signature is still correct for this build.
   # If not, issue a warning and skip making this signed target.  We
   # don't fail the build because this is a necessary step when the
   # cache must be updated; see link in message below.
   try:
      subprocess.check_output([sbverify,
                               '--detached', sname,
                               '--cert', topdir + '/env/' +
                               uefi_ca_cert[key][kind(iname)],
                               iname])
   except subprocess.CalledProcessError as e:
      if e.output == 'Signature verification failed\n':
         print('''Skipped signing with %s; cached signature is invalid
*** This build cannot be used in an official ESXi release.
*** See https://vmw-confluence.broadcom.net/display/WIKI/ESXSecureBoot-UEFI-CA-Signing''' % key)
         cleanup(iname, [key])
         exit(0)
      else:
         print(e.output)
         raise e

# Clean up temporary detached signature files
def cleanup(iname, keys):
   for key in keys:
      try:
         os.remove(signame(iname, key))
      except:
         pass

# Starting October 20, 2025, Microsoft will return two signed binaries
# for each approved submission, one signed with the existing 2011
# certificate and one with a new 2023 certificate.  Starting in June
# 2026, the 2011 certificate will have expired and only a 2023 signed
# binary will be returned.
#
# During the interim, we want to attach both signatures, with the 2011
# signature first just in case there are still any ESX supported
# machines with old UEFI firmware that can handle only one signature.
# Such old firmware seems more likely to trust the 2011 certificate
# than the new one.
#
# To avoid needing to change makefiles and change the names of
# deliverables, we temporarly take "uefi_ca" on the sign.py command
# line to mean attaching signatures with both 2011 and 2023
# certificates, by modifying argv[1]:
#
keysarg = sys.argv[1].replace('uefi_ca', 'uefi_ca2011+uefi_ca2023')

# Parse command line.
keys = keysarg.split('+')
iname = sys.argv[2]
if len(sys.argv) > 3:
   oname = sys.argv[3]
else:
   oname = iname + '-' + sys.argv[1]

# Generate a detached signature for each key.
for key in keys:
   vmware = key.find('vmware') >= 0
   uefi = key.find('uefi') >= 0
   if not official and vmware:
      print('Skipped signing with %s; test build' % key)
      cleanup(iname, keys)
      exit(0)
   if uefi:
      cachedsign(iname, key)
   elif vmware:
      remotesign(iname, key)
   else:
      localsign(iname, key)

# Attach the signatures
subprocess.check_call(['cp', '-f', iname, oname])
for key in keys:
   subprocess.check_call([sbattach,
                          '--attach', signame(iname, key),
                          oname])

cleanup(iname, keys)
