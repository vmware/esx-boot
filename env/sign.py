#! /usr/bin/python

#*******************************************************************************
# Copyright (c) 2015-2018,2022 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

# Wrapper script around the signing process.
# Usage: sign.py key1+key2+...+keyn inputFile [outputFile]
# Environment variables:
#  SIGN_RELEASE_BINARIES, LOCALKEYS, SBSIGN, SBATTACH, SBVERIFY, SIGNC

import sys
import os
import subprocess

# Pick up optional environment variables.
official = os.getenv('SIGN_RELEASE_BINARIES') == '1'
sbsign = os.getenv('SBSIGN') or 'sbsign'
sbattach = os.getenv('SBATTACH') or 'sbattach'
sbverify = os.getenv('SBVERIFY') or 'sbverify'
signc = os.getenv('SIGNC') or 'signc'
topdir = os.getenv('TOPDIR') or '.'
sigcache = topdir + '/sigcache'
localkeys = os.getenv('LOCALKEYS') or topdir + '/localkeys'
uefi_ca_cert = topdir + '/env/MicCorUEFCA2011_2011-06-27.pem'

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
def cachedsign(iname):
   key = 'uefi_ca'
   cname = sigcache + '/' + os.path.basename(iname)
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
                               '--cert', uefi_ca_cert,
                               iname])
   except subprocess.CalledProcessError as e:
      if e.output == 'Signature verification failed\n':
         print('''Skipped signing with %s; cached signature is invalid
*** This build cannot be used in an official ESXi release.
*** See https://wiki.eng.vmware.com/ESXSecureBoot/UEFI-CA-Signing
''' % key)
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

# Parse command line.
keys = sys.argv[1].split('+')
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
      print('Skipped signing with %s; developer build' % key)
      cleanup(iname, keys)
      exit(0)
   if uefi:
      cachedsign(iname)
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
